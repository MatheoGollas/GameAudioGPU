using UnityEngine;

namespace UnityAudioGPU
{
    [RequireComponent(typeof(AudioSource))]
    public class SoundTracingEmitter : MonoBehaviour
    {
        private void OnEnable()
        {
            //TODO: When FMOD is implemented, subscribe to the event start and stop
            //For now, we can just register on enable and unregister on disable
            if(SoundTracingContext.Instance == null)
            {
                SoundTracingContext.OnInitialized += OnAudioStart;
            }
            else
            {
                OnAudioStart();
            }
        }

        private void OnDisable()
        {
            if(SoundTracingContext.Instance != null)
            {
                OnAudioStop();
            }
        }


        private void OnAudioStart()
        {
            SoundTracingContext.Instance.RegisterEmitter(this);
        }

        private void OnAudioStop()
        {
            SoundTracingContext.Instance.UnregisterEmitter(this);
        }

        public virtual void OnSoundTraceResult(SoundTraceResult result)
        {
            Debug.Log("Got my data!: " + result.didHit);
        }
    }
}