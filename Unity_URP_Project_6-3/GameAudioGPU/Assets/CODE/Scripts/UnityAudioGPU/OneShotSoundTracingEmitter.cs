using UnityEngine;
using FMODUnity;
using System;

namespace UnityAudioGPU
{
    public class OneShotSoundTracingEmitter : SoundTracingEmitter
    {
        private FMOD.Studio.EVENT_CALLBACK OnEventCallback;
        public void PlayOneShot()
        {
            if(!emitter) emitter = GetComponent<StudioEventEmitter>();
            emitter.Play();
            OnEventCallback = new FMOD.Studio.EVENT_CALLBACK(EmitterCallback);
            emitter.EventInstance.setCallback(OnEventCallback, FMOD.Studio.EVENT_CALLBACK_TYPE.STARTED | FMOD.Studio.EVENT_CALLBACK_TYPE.STOPPED);
            

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

        [AOT.MonoPInvokeCallback(typeof(FMOD.Studio.EVENT_CALLBACK))]
        static FMOD.RESULT EmitterCallback(FMOD.Studio.EVENT_CALLBACK_TYPE type, IntPtr instancePtr, IntPtr parameterPtr)
        {
            if (type == FMOD.Studio.EVENT_CALLBACK_TYPE.STOPPED)
            {
                Debug.Log("Event stopped");
            }
            return FMOD.RESULT.OK;
        }
    }
}