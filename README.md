# Camera Interface

Camera Detector Controller Interface Software

## Reporting Issues

If you encounter any problems or have questions about this project, please open an issue on the [GitHub Issues page](https://github.com/CaltechOpticalObservatories/camera-interface/issues). Your feedback helps us improve the project!

## Requirements

- **CMake** 3.12 or higher
- **cfitsio** and **CCFits** libraries (expected in `/usr/local/lib`)
- **gtest** (Google Test) library (needed to run unit tests)

### Controller Compatibility

| Archon Controllers                  | ARC Controllers                              |
|------------------------------------|----------------------------------------------|
| `g++ 8.1` or higher (and C++17)    | `g++ 8.3` (and C++17)                        |
|                                    | ARC API 3.6 and Arc66PCIe driver             |

## Build Instructions

1. **Change to the build directory:**

    ```bash
    $ cd build
    ```

2. **Start with a clean build:** Delete the contents of the build directory, including the `CMakeFiles/` subdirectory, but **not** the `.gitignore` file.

    ```bash
    $ rm -Rf *
    ```

3. **Create the Makefile by running CMake** (from the build directory):

   | Archon                 | ARC                              |
   |------------------------|----------------------------------|
   | `$ cmake ..`           | `$ cmake -DINTERFACE_TYPE=AstroCam ..` |

4. **Compile the sources:**

    ```bash
    $ make
    ```

5. **Run the Camera Server:**

    - **As a foreground process:**

        ```bash
        $ ../bin/camerad <file.cfg> --foreground
        ```

    - **As a daemon:**

        ```bash
        $ ../bin/camerad -d <file.cfg>
        ```

   *Replace `<file.cfg>` with an appropriate configuration file. See the example `.cfg` files in the `Config` and `Config/demo` directories.*

6. **(Optional) Run the Archon Emulator:**

    ```bash
    $ ../bin/emulator <file.cfg>
    ```

   *Note: The emulator software will only be compiled when `INTERFACE_TYPE` is set to Archon (default).*

7. **(Optional) Run Unit Tests:**

    ```bash
    $ ../bin/run_unit_tests
    ```

---

David Hale  
<dhale@astro.caltech.edu>