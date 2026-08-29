// MUSA (Moore Threads) Feasibility Study
// Analysis of GPU vendor support requirements

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace litt {

// =============================================================================
// MUSA (Moore Threads) Support Analysis
// =============================================================================

struct MUSAAnalysis {
    bool supported = false;
    std::string driver_status = "Unknown";
    std::string vulkan_driver = "Not found";
    std::string requirements = "";
    std::vector<std::string> blockers;
    
    void analyze() {
        // Check for MUSA GPU presence
        check_gpu_presence();
        
        // Check driver installation
        check_driver();
        
        // Check Vulkan support
        check_vulkan_support();
        
        // Compile results
        compile_results();
    }
    
private:
    void check_gpu_presence() {
        // On Windows, check device manager
        // On Linux, check /proc/driver/musa or lspci
#ifdef _WIN32
        // Check Windows Registry for MUSA devices
        // HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\PCI
        std::ifstream pci_dev("C:/Windows/System32/DriverStore/FileRepository/*.inf");
        if (pci_dev.good()) {
            // Parse for Moore Threads device IDs
            std::string content((std::istreambuf_iterator<char>(pci_dev)),
                               std::istreambuf_iterator<char>());
            if (content.find("Moore") != std::string::npos ||
                content.find("MT") != std::string::npos) {
                supported = true;
                driver_status = "GPU detected";
            }
        }
#else
        // Check Linux for MUSA devices
        std::ifstream lspci("/proc/driver/musa/version");
        if (lspci.good()) {
            supported = true;
            driver_status = "MUSA driver detected";
        }
        
        // Alternative: check lspci output
        std::ifstream dmesg("/var/log/dmesg");
        if (dmesg.good()) {
            std::string content((std::istreambuf_iterator<char>(dmesg)),
                               std::istreambuf_iterator<char>());
            if (content.find("Moore") != std::string::npos ||
                content.find("musa") != std::string::npos) {
                supported = true;
            }
        }
#endif
    }
    
    void check_driver() {
#ifdef _WIN32
        // Check for MUSA driver files
        std::wstring driver_path = L"C:\\Windows\\System32\\DriverStore\\FileRepository\\";
        // Check for .inf files containing Moore Threads
        // This requires registry access or file system scan
        driver_status = "Driver check pending - requires hardware";
#elif defined(__linux__)
        // Check kernel module
        std::ifstream modinfo("/proc/modules/musa");
        if (modinfo.good()) {
            driver_status = "MUSA kernel module loaded";
        } else {
            driver_status = "MUSA kernel module not found";
            blockers.push_back("MUSA kernel module not loaded");
        }
        
        // Check user-space driver
        std::ifstream sysfs("/sys/bus/pci/drivers/musa");
        if (sysfs.good()) {
            driver_status = "MUSA driver initialized";
        }
#endif
    }
    
    void check_vulkan_support() {
        // Check if MUSA provides Vulkan driver
        // This is vendor-specific and requires actual hardware
#ifdef _WIN32
        // Check Vulkan ICDDirectory for MUSA driver
        std::ifstream icd_file("C:/Windows/System32/vulkan-1.dll");
        if (icd_file.good()) {
            vulkan_driver = "vulkan-1.dll found";
        } else {
            vulkan_driver = "Vulkan driver not found";
            blockers.push_back("No Vulkan driver for MUSA");
        }
#else
        // Check for MUSA Vulkan driver in /usr/share/vulkan/icd.d/
        std::ifstream icd_file("/usr/share/vulkan/icd.d/musa_icd.json");
        if (icd_file.good()) {
            vulkan_driver = "MUSA Vulkan ICD found";
        } else {
            vulkan_driver = "MUSA Vulkan ICD not found";
            blockers.push_back("No Vulkan driver for MUSA");
        }
#endif
    }
    
    void compile_results() {
        if (!supported) {
            requirements = "MUSA GPU hardware required";
            blockers.push_back("No MUSA GPU detected");
        }
        
        if (vulkan_driver.find("not found") != std::string::npos) {
            blockers.push_back("Vulkan driver not installed");
        }
        
        if (!blockers.empty()) {
            supported = false;
        }
    }
};

// =============================================================================
// NNAPI (Android Neural Networks API) Support Analysis
// =============================================================================

struct NNAPIAnalysis {
    bool supported = false;
    std::string android_version = "Unknown";
    std::string nnapi_version = "Unknown";
    std::vector<std::string> hardware_accelerators;
    std::vector<std::string> blockers;
    
    void analyze() {
        // NNAPI is Android-only
#ifdef __ANDROID__
        check_android_version();
        check_nnapi_availability();
        check_hardware_accelerators();
        compile_results();
#endif
    }
    
private:
    void check_android_version() {
        // Android API level
        // __ANDROID_API__ is set at compile time
#ifdef __ANDROID_API__
        if (__ANDROID_API__ >= 28) { // Android 9 (Pie)
            android_version = "Android 9+ (API 28+)";
            supported = true;
        } else {
            android_version = "Android < 9";
            supported = false;
            blockers.push_back("NNAPI requires Android 9+");
        }
#else
        android_version = "Not Android";
        supported = false;
        blockers.push_back("NNAPI is Android-only");
#endif
    }
    
    void check_nnapi_availability() {
        // NNAPI is part of Android SDK
        // Check at runtime using ANeuralNetworks_getVersion()
        // For analysis, we assume it's available on Android 9+
        nnapi_version = "ANeuralNetworks 1.2+ (Android 10+)";
    }
    
    void check_hardware_accelerators() {
        // Check for available NNAPI accelerators
        // This requires runtime analysis
        hardware_accelerators.push_back("CPU (fallback)");
        hardware_accelerators.push_back("GPU (via ANeuralNetworksGpu)");
        hardware_accelerators.push_back("DSP (via ANeuralNetworksDsp)");
        hardware_accelerators.push_back("NPU (vendor-specific)");
        
        // Note: NPU support varies by device manufacturer
        // Common NPU vendors: Qualcomm (SNPE), HiSilicon (MIND), Google (Edge TPU)
    }
    
    void compile_results() {
        if (blockers.empty()) {
            requirements = "Android 9+ device with NNAPI support";
        }
    }
};

// =============================================================================
// NPU Acceleration (Ryzen AI / Intel AI Boost / Samsung NPU)
// =============================================================================

struct NPUAnalysis {
    bool supported = false;
    std::string vendor = "Unknown";
    std::string npu_name = "Unknown";
    std::vector<std::string> capabilities;
    std::vector<std::string> blockers;
    
    void analyze() {
        // Check for NPU presence
        check_npu_presence();
        
        // Check vendor-specific APIs
        check_vendor_apis();
        
        // Compile results
        compile_results();
    }
    
private:
    void check_npu_presence() {
#ifdef _WIN32
        // Check Windows for NPU devices
        // Devices with "NPU", "AI", "Neural" in device manager
        // This requires WinRT or WMI access
        std::ifstream wmi_result("C:/Windows/System32/wbem/wmic.exe output:text list");
        if (wmi_result.good()) {
            std::string content((std::istreambuf_iterator<char>(wmi_result)),
                               std::istreambuf_iterator<char>());
            
            if (content.find("NPU") != std::string::npos ||
                content.find("AI Boost") != std::string::npos ||
                content.find("Ryzen AI") != std::string::npos) {
                supported = true;
                vendor = "AMD/Intel";
                npu_name = "Ryzen AI / Intel AI Boost";
            }
        }
#else
        // Linux: Check /sys/bus/platform/devices for NPU
        std::ifstream npu_device("/sys/bus/platform/devices/npu0");
        if (npu_device.good()) {
            supported = true;
            vendor = "Linux";
            npu_name = "Generic NPU";
        }
        
        // Check for Qualcomm Hexagon
        std::ifstream hexagon("/sys/bus/platform/devices/hexagon");
        if (hexagon.good()) {
            supported = true;
            vendor = "Qualcomm";
            npu_name = "Hexagon DSP";
        }
        
        // Check for Samsung NPU
        std::ifstream samsung_npu("/sys/bus/platform/devices/samsung_npu");
        if (samsung_npu.good()) {
            supported = true;
            vendor = "Samsung";
            npu_name = "Samsung NPU";
        }
#endif
    }
    
    void check_vendor_apis() {
        // AMD: ROCm AI Runtime
        // Intel: OpenVINO
        // Qualcomm: SNPE (Snapdragon Neural Processing Engine)
        // Samsung: SLSI NPU SDK
        
        capabilities.push_back("INT8 inference");
        capabilities.push_back("FP16 inference");
        capabilities.push_back("Mixed precision");
        capabilities.push_back("Model quantization");
        
        // Note: Full API analysis requires vendor SDK installation
    }
    
    void compile_results() {
        if (!supported) {
            requirements = "NPU hardware required";
            blockers.push_back("No NPU detected");
        }
    }
};

// =============================================================================
// Feasibility Summary
// =============================================================================

struct FeasibilityReport {
    MUSAAnalysis musa;
    NNAPIAnalysis nnapi;
    NPUAnalysis npu;
    
    void generate() {
        musa.analyze();
        nnapi.analyze();
        npu.analyze();
    }
    
    void print_report() {
        std::cout << "=== Litt Engine Hardware Acceleration Feasibility ===\n\n";
        
        // MUSA
        std::cout << "MUSA (Moore Threads):\n";
        std::cout << "  Supported: " << (musa.supported ? "Yes" : "No") << "\n";
        std::cout << "  Driver: " << musa.driver_status << "\n";
        std::cout << "  Vulkan: " << musa.vulkan_driver << "\n";
        if (!musa.blockers.empty()) {
            std::cout << "  Blockers:\n";
            for (auto& b : musa.blockers) {
                std::cout << "    - " << b << "\n";
            }
        }
        std::cout << "  Requirements: " << musa.requirements << "\n\n";
        
        // NNAPI
        std::cout << "NNAPI (Android):\n";
        std::cout << "  Supported: " << (nnapi.supported ? "Yes" : "No") << "\n";
        std::cout << "  Android: " << nnapi.android_version << "\n";
        std::cout << "  Accelerators:\n";
        for (auto& acc : nnapi.hardware_accelerators) {
            std::cout << "    - " << acc << "\n";
        }
        if (!nnapi.blockers.empty()) {
            std::cout << "  Blockers:\n";
            for (auto& b : nnapi.blockers) {
                std::cout << "    - " << b << "\n";
            }
        }
        std::cout << "  Requirements: " << nnapi.requirements << "\n\n";
        
        // NPU
        std::cout << "NPU Acceleration:\n";
        std::cout << "  Supported: " << (npu.supported ? "Yes" : "No") << "\n";
        std::cout << "  Vendor: " << npu.vendor << "\n";
        std::cout << "  NPU: " << npu.npu_name << "\n";
        std::cout << "  Capabilities:\n";
        for (auto& cap : npu.capabilities) {
            std::cout << "    - " << cap << "\n";
        }
        if (!npu.blockers.empty()) {
            std::cout << "  Blockers:\n";
            for (auto& b : npu.blockers) {
                std::cout << "    - " << b << "\n";
            }
        }
        std::cout << "  Requirements: " << npu.requirements << "\n";
    }
};

// =============================================================================
// Exported Function
// =============================================================================

FeasibilityReport generate_feasibility_report() {
    FeasibilityReport report;
    report.generate();
    return report;
}

} // namespace litt
