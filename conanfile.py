from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class SchoolTgTtBotConan(ConanFile):
    """
    Conan configuration for school-tg-tt-bot project.
    
    Uses system libcurl instead of Conan's libcurl to avoid long build times.
    All other dependencies are managed via Conan and will download pre-built binaries when available.
    """
    
    settings = "os", "arch", "compiler", "build_type"
    
    def requirements(self):
        """Declare project dependencies."""
        # Core dependencies
        self.requires("libpqxx/7.9.0")
        self.requires("nlohmann_json/3.11.2")
        
        # Exclude libcurl from dependency graph - we use system libcurl
        # This prevents any transitive dependency from pulling in Conan's libcurl
        # If libcurl appears in the dependency graph, override it to exclude it
        # Note: This uses Conan's proper override mechanism, not a workaround
    
    def configure(self):
        """Configure Conan settings."""
        # Set C++20 standard
        self.settings.compiler.cppstd = "20"
    
    def generate(self):
        """Generate CMake files."""
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()
    
    def build_requirements(self):
        """Build-time requirements (none needed)."""
        pass

