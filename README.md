# 🐳 Mini-Docker

**Mini-Docker** is a minimalist container runtime implemented in C, aiming to emulate core functionalities of Docker. This project serves as an educational tool to understand the inner workings of containerization by building a simplified version of Docker from scratch.

## 🚀 Features

* **Containerization Basics**: Leverages Linux namespaces and cgroups to isolate processes.
* **Image Handling**: Parses and utilizes Docker images for container creation.
* **Process Management**: Manages container lifecycle including start, stop, and delete operations.
* **Resource Limiting**: Implements basic resource constraints using cgroups.
* **Networking**: Sets up isolated network environments for containers.

## 🛠️ Getting Started

### Prerequisites

* **Operating System**: Linux (with support for namespaces and cgroups)
* **Compiler**: GCC or any C99 compatible compiler
* **Privileges**: Root access may be required for certain operations

### Installation

1. **Clone the repository**:

   ```bash
   git clone https://github.com/Kimya-M/Mini-Docker.git
   cd Mini-Docker
   ```

2. **Build the project**:

   ```bash
   make
   ```

   This will compile the source code and generate the `mini-docker` executable.

### Usage

```bash
sudo ./mini-docker run <image> <command>
```

**Example**:

```bash
sudo ./mini-docker run ubuntu /bin/bash
```

This command will start a new container using the Ubuntu image and launch the Bash shell inside it.

## 📁 Project Structure

* `main.c`: Entry point of the application.
* `container.c`: Contains functions related to container lifecycle management.
* `image.c`: Handles image parsing and management.
* `namespace.c`: Implements namespace isolation.
* `cgroups.c`: Manages control groups for resource limiting.
* `network.c`: Sets up networking for containers.
* `utils.c`: Utility functions used across the project.
* `Makefile`: Build instructions.

## 🧪 Testing

To test the functionality:

1. **Run a container**:

   ```bash
   sudo ./mini-docker run alpine /bin/sh
   ```

2. **Verify isolation**: Inside the container, check PID, network interfaces, and mounted filesystems to ensure isolation.

3. **Resource limits**: Test CPU and memory constraints by running resource-intensive processes inside the container.

## 📚 Documentation

Detailed documentation is available in the `docs/` directory, covering:

* Architecture overview
* Component interactions
* Extension guidelines

## 🤝 Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository.

2. Create a new branch:

   ```bash
   git checkout -b feature-name
   ```

3. Make your changes and commit them:

   ```bash
   git commit -m "Add feature"
   ```

4. Push to your fork:

   ```bash
   git push origin feature-name
   ```

5. Open a pull request describing your changes.

## 📄 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgements

* Inspired by the official [Docker](https://www.docker.com/) project.
* References:

  * [Linux Containers](https://linuxcontainers.org/)
  * [Namespaces in operation](https://man7.org/linux/man-pages/man7/namespaces.7.html)
  * [Control Groups](https://man7.org/linux/man-pages/man7/cgroups.7.html

---

Feel free to customize this `README.md` to better fit the specifics of your project. If you need assistance with any particular section or further customization, don't hesitate to ask!
