using UnityEngine;
using FMODUnity;
using System;
using System.Collections.Generic;

namespace UnityAudioGPU
{
    [RequireComponent(typeof(StudioEventEmitter))]
    public abstract class SoundTracingEmitter : MonoBehaviour
    {
        protected StudioEventEmitter emitter;
        [SerializeField][Tooltip("0 means no filtering, 1 means full bandwidth filtering")][Range(0f,1f)] protected float occlusionLowpassFreqLinear = 0.75f;
        [SerializeField][Tooltip("0 means no filtering, 1 means full bandwidth filtering")][Range(0f,1f)] protected float obstructionLowpassFreqLinear = 0.75f;
        [Range(0.01f,3.14f)]public float obstructionConeAngleRad = Mathf.PI * 0.5f; 
#if SOUND_TRACING_DEBUG && UNITY_EDITOR
        protected SoundTraceResult[] debug;
#endif
        
        protected virtual void Awake()
        {
            if(!emitter) emitter = GetComponent<StudioEventEmitter>();
        }

        protected virtual void OnAudioStarted()
        {
            SoundTracingContext.Instance.RegisterEmitter(this);
        }

        protected virtual void OnAudioStopped()
        {
            SoundTracingContext.Instance.UnregisterEmitter(this);
        }

        public virtual void OnSoundTraceResult(SoundTraceResult[] result)
        {
#if SOUND_TRACING_DEBUG && UNITY_EDITOR
            debug = result;
#endif

            float directPath = 0;
            float ambiantPath = 0;
            float directPathMax = 0;
            float ambiantPathMax = 0;

            foreach (SoundTraceResult r in result)
            {
                float influence = Mathf.Abs(r.influence);

                directPathMax += influence;
                ambiantPathMax += 1 - influence;

                if(r.DidHit) continue;

                directPath += influence;
                ambiantPath += 1 - influence;
            }

            float occlusionRatio = 1f-(directPath / directPathMax);
            float obstructionRatio = 1f-(ambiantPath / ambiantPathMax);

            float occlusionValue = occlusionLowpassFreqLinear * occlusionRatio;
            float obstructionValue = obstructionLowpassFreqLinear * obstructionRatio;

            emitter.SetParameter("RaytracedOcclusion", occlusionValue);
            emitter.SetParameter("RaytracedObstruction", obstructionValue);
        }
    }
}