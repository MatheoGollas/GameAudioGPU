using UnityEngine;
using UnityAudioGPU;
using UnityEngine.Experimental.Rendering;

[RequireComponent(typeof(MeshRenderer), typeof(MeshFilter))]
public class DestructableAudioGeometry : MonoBehaviour
{
    [SerializeField] private KeyCode destroyKey = KeyCode.Space;
    private int[] meshInstanceIDs;
    private bool isEnabled = true;
    private Material mat;

    private void Start()
    {
        mat = GetComponent<Renderer>().material;

        if(SoundTracingContext.Instance)
            SoundTracingContext.Instance.AddMeshToBVH(GetComponent<MeshFilter>(), out meshInstanceIDs);
    }

    private void Update()
    {
        if(Input.GetKeyDown(destroyKey)) ToggleObject();
    }

    public void ToggleObject()
    {
        if(isEnabled) DestroyObject();
        else RespawnObject();

        isEnabled = !isEnabled;
    }

    private void DestroyObject()
    {
        if(SoundTracingContext.Instance)
            SoundTracingContext.Instance.RemoveMeshFromBVH(meshInstanceIDs);
        mat.SetFloat("_Opacity", 0.2f);
    } 

    private void RespawnObject()
    {
        if(SoundTracingContext.Instance)
            SoundTracingContext.Instance.AddMeshToBVH(GetComponent<MeshFilter>(), out meshInstanceIDs);
        mat.SetFloat("_Opacity", 1f);
    }
}