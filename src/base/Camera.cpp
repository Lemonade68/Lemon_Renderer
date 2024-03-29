#include "Camera.h"

Camera::Camera(glm::vec3 _position, float _fov, float _near, float _far, Camera::CameraType _type)
    : type(_type), position(_position), fov_y(_fov), zNear(_near), zFar(_far) {
    glm::vec3 target(0.f);//初始时看向原点
    cameraWorldCoords.back  = glm::normalize(position - target);
    cameraWorldCoords.right = glm::normalize(glm::cross(worldUp, cameraWorldCoords.back));
    cameraWorldCoords.up    = glm::normalize(glm::cross(cameraWorldCoords.back, cameraWorldCoords.right));
    updateViewMatrix();
    //updatePerspectiveMatrix(fov_y, SCR_WIDTH / SCR_HEIGHT, zNear, zFar);		//创建swapchain时初始化
}

void Camera::updateViewMatrix() {
    if (type == CameraType::firstperson) {
        matrices.view = glm::lookAt(position, position - cameraWorldCoords.back, worldUp);
    } else {
        //todo：lookat camera
    }
}

void Camera::updatePerspectiveMatrix(float fov, float aspect, float znear, float zfar) {
    fov_y                = fov;
    zNear                = znear;
    zFar                 = zfar;
    aspect_ratio         = aspect;
    matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);

    //默认使用vulkan，需要flipY
    matrices.perspective[1][1] *= -1.0f;
}

void Camera::Tick(float deltaTime) {
    if (enterCam) {
        if (type == CameraType::firstperson) {
            //修改朝向信息
            cameraWorldCoords.right = glm::normalize(glm::cross(worldUp, cameraWorldCoords.back));
            cameraWorldCoords.up    = glm::normalize(glm::cross(cameraWorldCoords.back, cameraWorldCoords.right));

            //修改位置信息
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

            // updateViewMatrix();		//修改camera的信息会导致view matrix的重建，但是只有当fov和aspect_ratio改变时才会影响perspectiveMatrix
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
        updateViewMatrix();//修改camera的信息会导致view matrix的重建，但是只有当fov和aspect_ratio改变时才会影响perspectiveMatrix
        changedView = false;
    }
}
