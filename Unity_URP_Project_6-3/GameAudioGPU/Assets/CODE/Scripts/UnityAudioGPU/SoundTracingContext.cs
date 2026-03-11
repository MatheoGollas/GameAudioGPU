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
        [SerializeField] private bool anyHitTrace = false;
        [SerializeField] private int frameSkips = 0;
        private int skipFrameLeft = 0;
        private IRayTracingAccelStruct rayTracingAccelStruct;
        private IRayTracingShader rayTracingShader;
        private CommandBuffer cb;
        private RayTracingContext rtContext;
        private List<SoundTracingEmitter> emitters = new(); //TODO: Optimize by seperating static and dynamic emitters
        private ComputeBuffer emitterPosBuffer;
        private static Transform listener;

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
        }

        private void Start()
        {
            RayTracingResources rtResources = new();
            bool result = rtResources.LoadFromRenderPipelineResources();
            if(!result) return;

            RayTracingBackend backend = RayTracingContext.IsBackendSupported(RayTracingBackend.Hardware) ? RayTracingBackend.Hardware : RayTracingBackend.Compute;
            rtContext = new(backend, rtResources);

            AccelerationStructureOptions options = new()
            {
                buildFlags = rayTracingBuildFlags,
                useCPUBuild = true
            };

            rayTracingAccelStruct = rtContext.CreateAccelerationStructure(options);

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

            rayTracingShader.SetAccelerationStructure(cb, "_AccelStruct", rayTracingAccelStruct);
            Graphics.ExecuteCommandBuffer(cb);
            cb.Clear();
            buildScratchBuffer?.Dispose();
            //emitterPosBuffer = new(emitters.Count, sizeof(float) * 3, ComputeBufferType.Default, ComputeBufferMode.Dynamic);
        }

        void OnDestroy()
        {
            rayTracingAccelStruct.Dispose();
            rtContext.Dispose();
            emitterPosBuffer?.Release();
        }

        private void Update()
        {
            if(skipFrameLeft > 0)
            {
                skipFrameLeft--;
                return;
            }
            else skipFrameLeft = frameSkips;
            
            if(emitters == null || listener == null) return;

            if(!SetupBuffer(out ComputeBuffer resultBuffer)) return;
            int numItems = emitters.Count;
            uint threadCount = (uint)Mathf.NextPowerOfTwo(numItems);
            GraphicsBuffer traceScratchBuffer = RayTracingHelper.CreateScratchBufferForTrace(rayTracingShader, threadCount, 1u ,1u);
            rayTracingShader.SetVectorParam(cb, Shader.PropertyToID("_ListenerPos"), listener.position);
            rayTracingShader.SetIntParam(cb, Shader.PropertyToID("_NumEmitters"), numItems);
            rayTracingShader.SetBufferParam(cb, Shader.PropertyToID("_EmitterPositions"), emitterPosBuffer);
            rayTracingShader.SetBufferParam(cb, Shader.PropertyToID("_RayResults"), resultBuffer);
            rayTracingShader.Dispatch(cb, traceScratchBuffer, threadCount, 1u, 1u);

            Graphics.ExecuteCommandBuffer(cb);
            cb.Clear();
            traceScratchBuffer?.Dispose();

            AsyncGPUReadback.Request(resultBuffer, (req) =>
            {
                if(req.hasError)
                {
                    Debug.LogError("GPU readback error");
                    return;
                }

                if(!anyHitTrace)
                {
                    NativeArray<SoundTraceResult> results = req.GetData<SoundTraceResult>();
                    for (int i = 0; i < emitters.Count; i++)
                    {
                        emitters[i].OnSoundTraceResult(results[i]);
                    }
                    results.Dispose();
                }
                else
                {
                    NativeArray<int> results = req.GetData<int>();
                    for (int i = 0; i < emitters.Count; i++)
                    {
                        emitters[i].OnSoundTraceResult(new SoundTraceResult { didHit = results[i] == 1 });
                    }
                    results.Dispose();
                }
                resultBuffer.Dispose();
                emitterPosBuffer.Dispose();
            });
        }

        private bool SetupBuffer(out ComputeBuffer resultBuffer)
        {
            if(emitters == null || emitters.Count == 0)
            {
                resultBuffer = null;
                return false;
            }
            int outputSize = anyHitTrace ? 4 : sizeof(float) * 8 + 4;
            emitterPosBuffer = new(emitters.Count, sizeof(float) * 3, ComputeBufferType.Default, ComputeBufferMode.Dynamic);
            resultBuffer = new(emitters.Count, outputSize, ComputeBufferType.Default, ComputeBufferMode.Dynamic);

            Vector3[] emitterPositions = new Vector3[emitters.Count];

            if(!anyHitTrace)
            {
                SoundTraceResult[] traceResults = new SoundTraceResult[emitters.Count];

                SoundTraceResult defaultResult = new()
                {
                    hitPoint_distance = Vector4.zero,
                    hitNormal_absorption = Vector4.zero,
                    didHit = false
                };
                for (int i = 0; i < emitters.Count; i++)
                {
                    emitterPositions[i] = emitters[i].transform.position;
                    traceResults[i] = defaultResult;
                }
                resultBuffer.SetData(traceResults);
            }
            else
            {
                int[] traceResults = new int[emitters.Count];
                for (int i = 0; i < emitters.Count; i++)
                {
                    emitterPositions[i] = emitters[i].transform.position;
                    traceResults[i] = 0;
                }
                resultBuffer.SetData(traceResults);
            }

            
            emitterPosBuffer.SetData(emitterPositions);
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
    }

    public struct SoundTraceResult
    {
        public Vector4 hitPoint_distance;
        public Vector4 hitNormal_absorption;
        public bool didHit;
    }
}