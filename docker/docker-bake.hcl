variable "GITHUB_SERVER_URL" {
    type    = string
    default = "https://github.com"
}

variable "GITHUB_REPOSITORY" {
    type    = string
    default = "kabot-io/kabot-zephyr"
}

target "images" {
    name = "kabot-${ubuntu_distro}-${ros_distro}"
    matrix = {
        ubuntu_distro = ["noble"]
        ros_distro    = ["jazzy", "kilted", "rolling"]
    }
    dockerfile = "docker/devcontainer.Dockerfile"
    context = "."
    tags = ["${replace(GITHUB_SERVER_URL, "https://", "")}/${GITHUB_REPOSITORY}/${ubuntu_distro}-${ros_distro}:latest"]
    args = {
        UBUNTU_DISTRO = "${ubuntu_distro}"
        ROS_DISTRO    = "${ros_distro}"
    }
}
