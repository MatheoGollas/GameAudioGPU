using UnityEngine;
using FMODUnity;

namespace UnityAudioGPU
{
    public class LoopingSoundTracingEmitter : SoundTracingEmitter
    {
        private void OnEnable()
        {
            if(!emitter) emitter = GetComponent<StudioEventEmitter>();
            emitter.Play();

            if(SoundTracingContext.Instance == null)
                SoundTracingContext.OnInitialized += OnAudioStarted;
            else
                OnAudioStarted();
        }

        private void OnDisable()
        {
            if(emitter) emitter.Stop();
            
            if(SoundTracingContext.Instance != null)
            {
                OnAudioStopped();
            }
        }
#if SOUND_TRACING_DEBUG && UNITY_EDITOR
        private void Update()
        {
            if(debug == null) return;

            foreach (SoundTraceResult r in debug)
            {
                float influenceAbs = Mathf.Abs(r.influence);
                Color rayColor = r.DidHit ? Color.red : new (0, influenceAbs, 0);
                Debug.DrawLine(transform.position, r.hitPosition, rayColor);
                Debug.DrawLine(r.hitPosition, SoundTracingContext.listener.position, rayColor);
            }
        }
#endif
    }
}