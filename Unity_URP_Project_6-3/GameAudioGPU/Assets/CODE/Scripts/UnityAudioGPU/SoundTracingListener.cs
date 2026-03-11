using UnityEngine;
namespace UnityAudioGPU
{
    public class SoundTracingListener : MonoBehaviour
    {
        private void OnEnable()
        {
            SoundTracingContext.SetListener(transform);
        }
    }
}