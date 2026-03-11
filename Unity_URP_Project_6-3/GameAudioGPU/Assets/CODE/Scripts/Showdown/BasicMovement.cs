using UnityEngine;

public class BasicMovement : MonoBehaviour
{
    [SerializeField] AnimationCurve curveX;
    [SerializeField] AnimationCurve curveY;
    [SerializeField] AnimationCurve curveZ;
    [SerializeField] float speed = 1f;
    Vector3 startPos;
    // Start is called once before the first execution of Update after the MonoBehaviour is created
    void Start()
    {
        startPos = transform.position;
    }
    

    // Update is called once per frame
    void Update()
    {
        float t = Time.time * speed;
        t = t - Mathf.Floor(t);
 
        transform.position = new Vector3(
            startPos.x + curveX.Evaluate(t),
            startPos.y + curveY.Evaluate(t),
            startPos.z + curveZ.Evaluate(t)
        );
    }

    private void OnDrawGizmos()
    {
        if(Application.isPlaying)
        {
            Gizmos.color = Color.red;
            Gizmos.DrawSphere(transform.position, 0.1f);
        }
    }
}
