using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.UnifiedRayTracing;
using UnityEngine.Experimental.Rendering;
using System.Collections.Generic;
using System;
using Unity.Collections;

namespace UnityAudioGPU
{
    public class SoundTracingContext : MonoBehaviour
    {
        public static event Action OnInitialized;
        public static SoundTracingContext Instance { get; private set; }

        [SerializeField] private BuildFlags rayTracingBuildFlags = BuildFlags.PreferFastTrace;
        [SerializeField] private int frameSkips = 0;
        [SerializeField][Min(0)][Tooltip("Note: will be used to power 2")] private int subSlicingPerEmitter = 1;
        [SerializeField][Min(1)][Tooltip("Note: will be set to the next power of 2")] private int raysPerSlice = 64;
        [SerializeField][Min(0)][Tooltip("Distance at which emitters will get their obstruction cone untouched. Any emitter closer than this will have a greater obstruction angle")] private float distanceForConeAngles = 10f;

        private int skipFrameLeft = 0;
        private IRayTracingAccelStruct rayTracingAccelStruct;
        private IRayTracingShader rayTracingShader;
        private CommandBuffer cb;
        private RayTracingContext rtContext;
        private List<SoundTracingEmitter> emitters = new();
        private ComputeBuffer emitterPosBuffer;
        public static Transform listener;
        private bool shouldResizeEmitters = false;
        private bool shouldUpdateBVH = false;
        private Vector4[] emitterPositionsArray;
        private ComputeBuffer pooledResultBuffer;
        private int pooledResultBufferSize;
        private bool readbackInFlight;
        private int cachedRaysPerEmitter;
        private Action<AsyncGPUReadbackRequest> onReadbackComplete;
        private SoundTraceResult[][] perEmitterResults;

        private static readonly int ListenerPosID = Shader.PropertyToID("_ListenerPos");
        private static readonly int NumEmittersID = Shader.PropertyToID("_NumEmitters");
        private static readonly int EmitterPositionsID = Shader.PropertyToID("_EmitterPositions");
        private static readonly int RayResultsID = Shader.PropertyToID("_RayResults");

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
            onReadbackComplete = OnReadbackComplete;

#if SOUND_TRACING_DEBUG && UNITY_EDITOR
            GlobalKeyword debugKeyword = GlobalKeyword.Create("SOUND_TRACING_DEBUG");
            Shader.EnableKeyword(debugKeyword);
#endif
        }

        private void Start()
        {
            Renderer[] meshRenderers = FindObjectsByType<Renderer>(FindObjectsSortMode.None);
            foreach (Renderer renderer in meshRenderers)
            {
                if(renderer.rayTracingMode != RayTracingMode.Static) continue; //Only bake static objects for now
                if(renderer.enabled == false) continue;
                if(renderer.gameObject.activeSelf == false) continue;
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

            cb = new()
            {
                name = "Sound Tracing Cmd Buffer"
            };
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
            pooledResultBuffer?.Release();
        }

        private void Update()
        {
            if(--skipFrameLeft > 0) return;
            else skipFrameLeft = frameSkips;
            
            if(emitters == null || listener == null) return;
            if(readbackInFlight) return;

            if(shouldUpdateBVH) UpdateBVH();

            int numItems = emitters.Count;
            int threadCountX = /*Mathf.NextPowerOfTwo(numItems)*/numItems;
            uint threadCountY = 1u << subSlicingPerEmitter;
            int threadCountZ = /*Mathf.NextPowerOfTwo(raysPerSlice)*/raysPerSlice;

            if(!SetupBuffer(new Vector3Int(threadCountX, (int)threadCountY, threadCountZ))) return;

            GraphicsBuffer traceScratchBuffer = RayTracingHelper.CreateScratchBufferForTrace(rayTracingShader, (uint)threadCountX, threadCountY, (uint)threadCountZ);
            rayTracingShader.SetVectorParam(cb, ListenerPosID, listener.position);
            rayTracingShader.SetIntParam(cb, NumEmittersID, numItems);
            rayTracingShader.SetBufferParam(cb, EmitterPositionsID, emitterPosBuffer);
            rayTracingShader.SetBufferParam(cb, RayResultsID, pooledResultBuffer);
            rayTracingShader.Dispatch(cb, traceScratchBuffer, (uint)threadCountX, threadCountY, (uint)threadCountZ);

            Graphics.ExecuteCommandBuffer(cb);
            cb.Clear();
            traceScratchBuffer?.Dispose();

            cachedRaysPerEmitter = (int)threadCountY * threadCountZ;
            readbackInFlight = true;
            AsyncGPUReadback.Request(pooledResultBuffer, onReadbackComplete);
        }

        private void OnReadbackComplete(AsyncGPUReadbackRequest req)
        {
            readbackInFlight = false;

            if(req.hasError)
            {
                Debug.LogError("GPU readback error");
                return;
            }

            int raysPerEmitter = cachedRaysPerEmitter;
            int emitterCount = emitters.Count;

            if(perEmitterResults == null || perEmitterResults.Length != emitterCount)
                perEmitterResults = new SoundTraceResult[emitterCount][];

#if SOUND_TRACING_DEBUG && UNITY_EDITOR
            NativeArray<Vector4> results = req.GetData<Vector4>();

            for (int i = 0; i < emitterCount; i++)
            {
                if(perEmitterResults[i] == null || perEmitterResults[i].Length != raysPerEmitter)
                    perEmitterResults[i] = new SoundTraceResult[raysPerEmitter];

                for (int j = 0; j < raysPerEmitter; j++)
                {
                    int idx = i * raysPerEmitter + j;
                    perEmitterResults[i][j] = new SoundTraceResult { influence = results[idx].w, hitPosition = new Vector3(results[idx].x, results[idx].y, results[idx].z) };
                }
                emitters[i].OnSoundTraceResult(perEmitterResults[i]);
            }
#else
            NativeArray<float> results = req.GetData<float>();
            for (int i = 0; i < emitterCount; i++)
            {
                if(perEmitterResults[i] == null || perEmitterResults[i].Length != raysPerEmitter)
                    perEmitterResults[i] = new SoundTraceResult[raysPerEmitter];

                for (int j = 0; j < raysPerEmitter; j++)
                {
                    int idx = i * raysPerEmitter + j;
                    perEmitterResults[i][j] = new SoundTraceResult { influence = results[idx] };
                }
                emitters[i].OnSoundTraceResult(perEmitterResults[i]);
            }
#endif
        }

        private void UpdateBVH()
        {
            GraphicsBuffer buildScratchBuffer = RayTracingHelper.CreateScratchBufferForBuild(rayTracingAccelStruct);
            rayTracingAccelStruct.Build(cb, buildScratchBuffer);
            Graphics.ExecuteCommandBuffer(cb);
            cb.Clear();
            buildScratchBuffer?.Dispose();
        }

        private bool SetupBuffer(Vector3Int dispatchSize)
        {               
            int numRays = dispatchSize.x * dispatchSize.y * dispatchSize.z;
            if(emitters == null || emitters.Count == 0) return false;

#if SOUND_TRACING_DEBUG && UNITY_EDITOR
            int sizeOfOutput = sizeof(float) * 4;
#else
            int sizeOfOutput = sizeof(float);
#endif

            if(pooledResultBuffer == null || pooledResultBufferSize != numRays)
            {
                pooledResultBuffer?.Release();
                pooledResultBuffer = new ComputeBuffer(numRays, sizeOfOutput, ComputeBufferType.Default, ComputeBufferMode.Dynamic);
                pooledResultBufferSize = numRays;
            }

            if(shouldResizeEmitters || emitterPosBuffer == null)
            {
                emitterPosBuffer?.Release();
                emitterPosBuffer = new ComputeBuffer(emitters.Count, sizeof(float) * 4, ComputeBufferType.Default, ComputeBufferMode.Dynamic);
                shouldResizeEmitters = false;
            }

            int count = emitters.Count;
            if(emitterPositionsArray == null || emitterPositionsArray.Length != count)
                emitterPositionsArray = new Vector4[count];

            float invertDist = 1f / distanceForConeAngles;
            for (int i = 0; i < count; i++)
            {
                Vector3 pos = emitters[i].transform.position;
                float interp = Mathf.Clamp01(Vector3.Distance(pos, listener.position) * invertDist);
                float w = Mathf.Lerp(3.0f, emitters[i].obstructionConeAngleRad, interp);
                emitterPositionsArray[i] = new Vector4(pos.x, pos.y, pos.z, w);
            }
            emitterPosBuffer.SetData(emitterPositionsArray);

            return true;
        } 

        public void RegisterEmitter(SoundTracingEmitter newEmitter)
        {
            if(newEmitter == null) return;
            emitters.Add(newEmitter);
            shouldResizeEmitters = true;
        }

        public void UnregisterEmitter(SoundTracingEmitter emitterToRemove)
        {
            if(emitterToRemove == null) return;
            emitters.Remove(emitterToRemove);
            shouldResizeEmitters = true;
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

#if SOUND_TRACING_DEBUG && UNITY_EDITOR
        public Vector3 hitPosition;
#endif

        public readonly bool DidHit => influence >= 0;
        public readonly float GetInfluence => Mathf.Abs(influence);
    }
}