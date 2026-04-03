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

        private void Update()
        {
            if(debug == null) return;

            foreach (SoundTraceResult r in debug)
            {
                // Debug.DrawRay(transform.position, r.hitPosition);
                Debug.DrawLine(transform.position, r.hitPosition);
            }
        }
    }
}