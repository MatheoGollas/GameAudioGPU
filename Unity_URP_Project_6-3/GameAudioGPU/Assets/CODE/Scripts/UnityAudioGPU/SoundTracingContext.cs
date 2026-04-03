using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.UnifiedRayTracing;
using UnityEngine.Experimental.Rendering;
using System.Collections.Generic;
using System;
using Unity.Collections;
using System.Linq;

namespace UnityAudioGPU
{
    public class SoundTracingContext : MonoBehaviour
    {
        public static event Action OnInitialized;
        public static SoundTracingContext Instance { get; private set; }
        
        //[SerializeField] private bool anyHitTrace = false;
        [SerializeField] private BuildFlags rayTracingBuildFlags = BuildFlags.PreferFastTrace;
        [SerializeField] private int frameSkips = 0;
        [SerializeField][Min(0)][Tooltip("Note: will be used to power 2")] private int subSlicingPerEmitter = 1;
        [SerializeField][Min(1)][Tooltip("Note: will be set to the next power of 2")] private int raysPerSlice = 64;

        private int skipFrameLeft = 0;
        private IRayTracingAccelStruct rayTracingAccelStruct;
        private IRayTracingShader rayTracingShader;
        private CommandBuffer cb;
        private RayTracingContext rtContext;
        private List<SoundTracingEmitter> emitters = new();
        private ComputeBuffer emitterPosBuffer;
        private static Transform listener;
        private bool shouldUpdateBVH = false;

        private void Awake()
        {
            if(Instance != null)
            {
                Destroy(this);
                return;
            }
            Instance = this;
            OnInitialized?.Invoke();
            OnInitialized = null;
            
            RayTracingResources rtResources = new();
            bool result = rtResources.LoadFromRenderPipelineResources();
            if(!result) return;

            RayTracingBackend backend = RayTracingContext.IsBackendSupported(RayTracingBackend.Hardware) ? RayTracingBackend.Hardware : RayTracingBackend.Compute;
            rtContext = new(backend, rtResources);
            
            Debug.Log("Using ray tracing backend: " + backend);

            AccelerationStructureOptions options = new()
            {
                buildFlags = rayTracingBuildFlags,
                useCPUBuild = true
            };

            rayTracingAccelStruct = rtContext.CreateAccelerationStructure(options);
        }

        private void Start()
        {
            Renderer[] meshRenderers = FindObjectsByType<Renderer>(FindObjectsSortMode.None);
            foreach (Renderer renderer in meshRenderers)
            {
                if(renderer.rayTracingMode != RayTracingMode.Static) continue; //Only bake static objects for now
                Mesh mesh = renderer.GetComponent<MeshFilter>().sharedMesh;
                if (mesh == null) continue;
                int subMeshCount = mesh.subMeshCount;

                for (int i = 0; i < subMeshCount; ++i)
                {
                    MeshInstanceDesc instanceDesc = new(mesh, i)
                    {
                        localToWorldMatrix = renderer.transform.localToWorldMatrix
                    };
                    rayTracingAccelStruct.AddInstance(instanceDesc);
                }
            }

            RayTracingShader shader = Resources.Load<RayTracingShader>("SoundTracing");
            rayTracingShader = rtContext.CreateRayTracingShader(shader);

            cb = new();
            GraphicsBuffer buildScratchBuffer = RayTracingHelper.CreateScratchBufferForBuild(rayTracingAccelStruct);
            rayTracingAccelStruct.Build(cb, buildScratchBuffer);
            
            shouldUpdateBVH = false;

            rayTracingShader.SetAccelerationStructure(cb, "_AccelStruct", rayTracingAccelStruct);
            Graphics.ExecuteCommandBuffer(cb);
            cb.Clear();
            buildScratchBuffer?.Dispose();
        }

        void OnDestroy()
        {
            rayTracingAccelStruct.Dispose();
            rtContext.Dispose();
            emitterPosBuffer?.Release();
        }

        private void Update()
        {
            if(--skipFrameLeft > 0) return;
            else skipFrameLeft = frameSkips;
            
            if(emitters == null || listener == null) return;

            if(shouldUpdateBVH) UpdateBVH();

            int numItems = emitters.Count;
            int threadCountX = Mathf.NextPowerOfTwo(numItems);
            uint threadCountY = 1u << subSlicingPerEmitter;
            int threadCountZ = Mathf.NextPowerOfTwo(raysPerSlice);

            if(!SetupBuffer(out ComputeBuffer resultBuffer, new Vector3Int(threadCountX, (int)threadCountY, threadCountZ))) return;

            GraphicsBuffer traceScratchBuffer = RayTracingHelper.CreateScratchBufferForTrace(rayTracingShader, (uint)threadCountX, threadCountY, (uint)threadCountZ);
            rayTracingShader.SetVectorParam(cb, Shader.PropertyToID("_ListenerPos"), listener.position);
            rayTracingShader.SetIntParam(cb, Shader.PropertyToID("_NumEmitters"), numItems);
            rayTracingShader.SetBufferParam(cb, Shader.PropertyToID("_EmitterPositions"), emitterPosBuffer);
            rayTracingShader.SetBufferParam(cb, Shader.PropertyToID("_RayResults"), resultBuffer);
            rayTracingShader.Dispatch(cb, traceScratchBuffer, (uint)threadCountX, threadCountY, (uint)threadCountZ);

            Graphics.ExecuteCommandBuffer(cb);
            cb.Clear();
            traceScratchBuffer?.Dispose();

            AsyncGPUReadback.Request(resultBuffer, (req) =>
            {
                if(req.hasError)
                {
                    Debug.LogError("GPU readback error");
                    resultBuffer.Dispose();
                    emitterPosBuffer.Dispose();
                    return;
                }

                NativeArray<Vector4> results = req.GetData<Vector4>();
                int raysPerEmitter = (int)threadCountY * threadCountZ;
                for (int i = 0; i < emitters.Count; i++)
                {
                    SoundTraceResult[] emitterResults = new SoundTraceResult[raysPerEmitter];
                    for (int j = 0; j < raysPerEmitter; j++)
                    {
                        emitterResults[j] = new SoundTraceResult { influence = results[i * raysPerEmitter + j].w, hitPosition = new Vector3(results[i * raysPerEmitter + j].x, results[i * raysPerEmitter + j].y, results[i * raysPerEmitter + j].z) };
                        /*Vector3 debugPos = new Vector3(results[i * raysPerEmitter + j].x, results[i * raysPerEmitter + j].y, results[i * raysPerEmitter + j].z);
                        Debug.DrawRay(emitters[i].transform.position, debugPos);*/
                    }
                    emitters[i].OnSoundTraceResult(emitterResults);
                }

                results.Dispose();
                
                resultBuffer.Dispose();
                emitterPosBuffer.Dispose();
            });
        }

        private void UpdateBVH()
        {
            GraphicsBuffer buildScratchBuffer = RayTracingHelper.CreateScratchBufferForBuild(rayTracingAccelStruct);
            rayTracingAccelStruct.Build(cb, buildScratchBuffer);
            Graphics.ExecuteCommandBuffer(cb);
            cb.Clear();
            buildScratchBuffer?.Dispose();
        }

        private bool SetupBuffer(out ComputeBuffer resultBuffer, Vector3Int dispatchSize)
        {
            int numRays = dispatchSize.x * dispatchSize.y * dispatchSize.z;
            if(emitters == null || emitters.Count == 0)
            {
                resultBuffer = null;
                return false;
            }
            emitterPosBuffer = new(emitters.Count, sizeof(float) * 4, ComputeBufferType.Default, ComputeBufferMode.Dynamic);
            resultBuffer = new(numRays, sizeof(float) * 4, ComputeBufferType.Default, ComputeBufferMode.Dynamic);

            Vector4[] emitterPositions = new Vector4[emitters.Count];
            for (int i = 0; i < emitters.Count; i++)
            {
                Vector3 pos = emitters[i].transform.position;
                float w = emitters[i].obstructionConeAngleRad;

                emitterPositions[i] = new Vector4(pos.x, pos.y, pos.z, w);
            }
            emitterPosBuffer.SetData(emitterPositions);
            
            Vector4[] traceResults = Enumerable.Repeat(Vector4.zero, numRays).ToArray();
            resultBuffer.SetData(traceResults);

            return true;
        } 

        public void RegisterEmitter(SoundTracingEmitter newEmitter)
        {
            if(newEmitter == null) return;
            emitters.Add(newEmitter);
        }

        public void UnregisterEmitter(SoundTracingEmitter emitterToRemove)
        {
            if(emitterToRemove == null) return;
            emitters.Remove(emitterToRemove);
        }

        public static void SetListener(Transform newListener)
        {
            listener = newListener;
        }

        public bool AddMeshToBVH(MeshFilter meshFilter, out int[] instanceID)
        {
            Mesh mesh = meshFilter.sharedMesh;
            if (mesh == null)
            {
                instanceID = null;
                return false;
            }
            int subMeshCount = mesh.subMeshCount;
            instanceID = new int[subMeshCount];

            for (int i = 0; i < subMeshCount; ++i)
            {
                MeshInstanceDesc instanceDesc = new(mesh, i)
                {
                    localToWorldMatrix = meshFilter.transform.localToWorldMatrix
                };
                instanceID[i] = rayTracingAccelStruct.AddInstance(instanceDesc);
            }

            shouldUpdateBVH = true;
            return true;
        }

        public void RemoveMeshFromBVH(int[] instanceID)
        {
            foreach(int id in instanceID)
            {
                rayTracingAccelStruct.RemoveInstance(id);
            }
            shouldUpdateBVH = true;
        }

        public void UpdateMeshFromBVH(Matrix4x4 transform, int[] instanceID)
        {
            foreach(int id in instanceID)
            {
                rayTracingAccelStruct.UpdateInstanceTransform(id, transform);
            }
            shouldUpdateBVH = true;
        }
    }

    public struct SoundTraceResult
    {
        public float influence;
        public Vector3 hitPosition;
        public readonly bool DidHit => influence >= 0;
    }
}