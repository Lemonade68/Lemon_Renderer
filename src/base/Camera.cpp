#include "Camera.h"

Camera::Camera(glm::vec3 _position, float _fov, float _near, float _far, Camera::CameraType _type)
    : type(_type), position(_position), fov_y(_fov), zNear(_near), zFar(_far) {
    glm::vec3 target(0.f);//��ʼʱ����ԭ��
    cameraWorldCoords.back  = glm::normalize(position - target);
    cameraWorldCoords.right = glm::normalize(glm::cross(worldUp, cameraWorldCoords.back));
    cameraWorldCoords.up    = glm::normalize(glm::cross(cameraWorldCoords.back, cameraWorldCoords.right));
    updateViewMatrix();
    //updatePerspectiveMatrix(fov_y, SCR_WIDTH / SCR_HEIGHT, zNear, zFar);		//����swapchainʱ��ʼ��
}

void Camera::updateViewMatrix() {
    if (type == CameraType::firstperson) {
        matrices.view = glm::lookAt(position, position - cameraWorldCoords.back, worldUp);
    } else {
        //todo��lookat camera
    }
}

void Camera::updatePerspectiveMatrix(float fov, float aspect, float znear, float zfar) {
    fov_y                = fov;
    zNear                = znear;
    zFar                 = zfar;
    aspect_ratio         = aspect;
    matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);

    //Ĭ��ʹ��vulkan����ҪflipY
    matrices.perspective[1][1] *= -1.0f;
}

void Camera::Tick(float deltaTime) {
    if (enterCam) {
        if (type == CameraType::firstperson) {
            //�޸ĳ�����Ϣ
            cameraWorldCoords.right = glm::normalize(glm::cross(worldUp, cameraWorldCoords.back));
            cameraWorldCoords.up    = glm::normalize(glm::cross(cameraWorldCoords.back, cameraWorldCoords.right));

            //�޸�λ����Ϣ
            float moveSpeed = deltaTime * movementSpeed;
            if (keys.forward) {
                position -= cameraWorldCoords.back * moveSpeed;
                changedView = true;
            }
            if (keys.backward) {
                position += cameraWorldCoords.back * moveSpeed;
                changedView = true;
            }
            if (keys.left) {
                position -= cameraWorldCoords.right * moveSpeed;
                changedView = true;
            }
            if (keys.right) {
                position += cameraWorldCoords.right * moveSpeed;
                changedView = true;
            }
            if (keys.up) {
                position += cameraWorldCoords.up * moveSpeed;
                changedView = true;
            }
            if (keys.down) {
                position -= cameraWorldCoords.up * moveSpeed;
                changedView = true;
            }

            // updateViewMatrix();		//�޸�camera����Ϣ�ᵼ��view matrix���ؽ�������ֻ�е�fov��aspect_ratio�ı�ʱ�Ż�Ӱ��perspectiveMatrix
        }
    }
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    cameraWorldCoords.back = glm::normalize(-front);

    // to be optimized
    if(changedPerspect){
        updatePerspectiveMatrix(fov_y, aspect_ratio, zNear, zFar);
        changedPerspect = false;
    }
    if(changedView){
        updateViewMatrix();//�޸�camera����Ϣ�ᵼ��view matrix���ؽ�������ֻ�е�fov��aspect_ratio�ı�ʱ�Ż�Ӱ��perspectiveMatrix
        changedView = false;
    }
}
