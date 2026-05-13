import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import get, copy, rmdir

required_conan_version = ">=2.0.0"


class Secp256k1Conan(ConanFile):
    name = "secp256k1"
    description = "Optimized C library for ECDSA operations on curve secp256k1"
    url = "https://github.com/bitcoin-core/secp256k1"
    license = "MIT"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "enable_module_recovery": [True, False],
        "enable_module_schnorrsig": [True, False],
        "enable_module_ecdh": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "enable_module_recovery": True,
        "enable_module_schnorrsig": True,
        "enable_module_ecdh": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SECP256K1_BUILD_TESTS"] = False
        tc.variables["SECP256K1_BUILD_EXHAUSTIVE_TESTS"] = False
        tc.variables["SECP256K1_BUILD_BENCHMARK"] = False
        tc.variables["SECP256K1_BUILD_EXAMPLES"] = False
        tc.variables["SECP256K1_ENABLE_MODULE_RECOVERY"] = self.options.enable_module_recovery
        tc.variables["SECP256K1_ENABLE_MODULE_SCHNORRSIG"] = self.options.enable_module_schnorrsig
        tc.variables["SECP256K1_ENABLE_MODULE_ECDH"] = self.options.enable_module_ecdh
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.libs = ["secp256k1"]
        self.cpp_info.set_property("cmake_file_name", "secp256k1")
        self.cpp_info.set_property("cmake_target_name", "secp256k1::secp256k1")
        self.cpp_info.set_property("pkg_config_name", "libsecp256k1")
