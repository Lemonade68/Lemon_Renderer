#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
private:
	glm::vec3 position;
	glm::vec3 worldUp{ 0.f, 1.f, 0.f };

public:
	struct {
		glm::vec3 right;	//x，cam的右方
		glm::vec3 up;		//y，摄像机的上方
		glm::vec3 back;		//z，对应摄像机朝向的反方向，即相机朝向为-back
	} cameraWorldCoords;

	bool enterCam = true;
	bool firstEnter = true;

	float fov_y;
	float zNear, zFar;
	float aspect_ratio;
	float pitch = 0.f, yaw = -90.f;				//俯仰角与偏航角(初始指向-z轴)

	enum class CameraType {
		firstperson,
		lookat
	};
	CameraType type;		

	struct {
		glm::mat4 view;				//view矩阵
		glm::mat4 perspective;		//perspective矩阵
	} matrices;

	struct {
		bool left = false;
		bool right = false;
		bool forward = false;
		bool backward = false;
		bool up = false;
		bool down = false;
	} keys;

	float movementSpeed = 2.f;
	float rotationSpeed = 0.3f;

public:
    Camera(glm::vec3 _position = glm::vec3(0.f, 0.f, 3.f),
           float _fov = 45.f,
           float _near = 0.1f,
           float _far = 256.f,
           Camera::CameraType _type = Camera::CameraType::firstperson);

    void updateViewMatrix();

    void updatePerspectiveMatrix(float fov, float aspect, float znear, float zfar);

    void Tick(float deltaTime);

    static Camera &GetInstance();
};

inline Camera& Camera::GetInstance(){
    static Camera camera;
    return camera;
}

#endif