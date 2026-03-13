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

        public virtual void OnSoundTraceResult(SoundTraceResult result)
        {
            emitter.SetParameter("RaytracedOcclusion", result.didHit ? occlusionLowpassFreqLinear : 0f, false);
        }
    }
}