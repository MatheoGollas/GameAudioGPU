using UnityEngine;
using UnityAudioGPU;
using UnityEngine.Experimental.Rendering;

[RequireComponent(typeof(MeshRenderer), typeof(MeshFilter))]
public class DynamicAudioGeometry : MonoBehaviour
{
    [SerializeField] private bool autoUpdate = true;
    private Vector3 prevPos;
    private int[] meshInstanceIDs = new int[0];
    private void Awake()
    {
        Renderer renderer = GetComponent<MeshRenderer>();
        if(renderer.rayTracingMode == RayTracingMode.Off || renderer.rayTracingMode == RayTracingMode.Static)
        {
            renderer.rayTracingMode = RayTracingMode.DynamicTransform;
        }
        prevPos = transform.position;
    }

    private void Start()
    {
        if(SoundTracingContext.Instance)
        {
            MeshFilter mf = GetComponent<MeshFilter>();
            SoundTracingContext.Instance.AddMeshToBVH(mf, out meshInstanceIDs);
        } 
    }

    private void Update()
    {
        if(!autoUpdate) return;
        if(transform.position != prevPos)
        {
            UpdateBVH();
            prevPos = transform.position;
        }
    }

    public void UpdateBVH()
    {
        if(SoundTracingContext.Instance) SoundTracingContext.Instance.UpdateMeshFromBVH(transform.localToWorldMatrix, meshInstanceIDs);
    }
}
