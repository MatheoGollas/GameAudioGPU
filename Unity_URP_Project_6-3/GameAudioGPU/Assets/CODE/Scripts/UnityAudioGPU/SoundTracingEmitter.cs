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
        [SerializeField][Tooltip("How much does low influence rays affect the sound")][Range(0f,1f)] protected float lowInfluenceMultiplier = 0.35f;
        [Range(0.01f,3.0f)][Tooltip("The angle of the obstruction cone in radians, at the distance specified in the SoundTracingContext")]public float obstructionConeAngleRad = Mathf.PI * 0.5f;
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

            // if(obstructionConeAngleRad < 0.5f)
                // occlusionRatio = 0.5f - Mathf.Sin(Mathf.Asin(1f-2f*occlusionRatio)*0.33333333f); //inverse smoothstep from https://iquilezles.org/articles/ismoothstep/#:~:text=Intro,Q%20=%20p/3
            
            occlusionRatio = 1f-occlusionRatio;
            occlusionRatio *= occlusionRatio;
            occlusionRatio = 1f-occlusionRatio;

            obstructionRatio = 1f-obstructionRatio;
            obstructionRatio *= obstructionRatio;
            obstructionRatio *= obstructionRatio;
            obstructionRatio = 1f-obstructionRatio; // inverse square falloff

            float finalRatio = (occlusionRatio + obstructionRatio) / (1f+lowInfluenceMultiplier);

            float occlusionValue = occlusionLowpassFreqLinear * finalRatio;
            
            emitter.SetParameter("RaytracedOcclusion", occlusionValue);
        }
    }
}