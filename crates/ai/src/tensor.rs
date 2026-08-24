//! Unified tensor representation for AI inference.
//! Supports multiple precision modes and data layouts.

use super::selector::BackendKind;

/// Data type for tensor elements
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub enum DataType {
    #[default]
    Float32,
    Float16,
    Int8,
    Uint8,
    Int32,
    Bf16,
}

impl DataType {
    /// Size in bytes
    pub fn size(&self) -> usize {
        match self {
            Self::Float32 => 4,
            Self::Float16 => 2,
            Self::Int8 => 1,
            Self::Uint8 => 1,
            Self::Int32 => 4,
            Self::Bf16 => 2,
        }
    }

    /// Check if this type is supported by the backend
    pub fn is_supported(&self, backend: &BackendKind) -> bool {
        match self {
            Self::Float32 => true, // All backends support FP32
            Self::Float16 => matches!(backend, BackendKind::Gpu | BackendKind::Npu(_) | BackendKind::Cpu),
            Self::Int8 => matches!(backend, BackendKind::Npu(_) | BackendKind::Cpu),
            Self::Uint8 => matches!(backend, BackendKind::Npu(_) | BackendKind::Cpu),
            Self::Int32 => true,
            Self::Bf16 => matches!(backend, BackendKind::Gpu | BackendKind::Npu(_)),
        }
    }
}

/// Tensor shape (NCHW or NHWC layout)
#[derive(Clone, Debug, Default)]
pub struct Shape {
    pub dims: Vec<u32>,
    pub layout: TensorLayout,
}

impl Shape {
    /// Create a shape from dimensions
    pub fn new(dims: &[u32]) -> Self {
        Self { dims: dims.to_vec(), layout: TensorLayout::NCHW }
    }

    /// Create a batch of images (NHWC layout)
    pub fn image_batch(batch: u32, height: u32, width: u32, channels: u32) -> Self {
        Self {
            dims: vec![batch, height, width, channels],
            layout: TensorLayout::NHWC,
        }
    }

    /// Total number of elements
    pub fn num_elements(&self) -> u64 {
        self.dims.iter().map(|&d| d as u64).product()
    }

    /// Total size in bytes
    pub fn size_bytes(&self, data_type: DataType) -> usize {
        (self.num_elements() * data_type.size() as u64) as usize
    }
}

/// Tensor data layout
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub enum TensorLayout {
    #[default]
    NCHW, // Channels first (CNN typical)
    NHWC, // Channels last (TensorFlow/ML typical)
}

/// A tensor -- unified representation for all backends
#[derive(Debug)]
pub struct Tensor {
    pub shape: Shape,
    pub data_type: DataType,
    pub data: Vec<u8>,
    pub device: TensorDevice,
}

impl Tensor {
    /// Create a new empty tensor
    pub fn empty(shape: Shape, data_type: DataType) -> Self {
        let size = shape.size_bytes(data_type);
        Self {
            shape,
            data_type,
            data: vec![0u8; size],
            device: TensorDevice::CPU,
        }
    }

    /// Create a tensor from raw data
    pub fn from_data(data: Vec<u8>, shape: Shape, data_type: DataType) -> Self {
        Self { data, shape, data_type, device: TensorDevice::CPU }
    }

    /// Create a tensor from float values
    pub fn from_floats(values: &[f32], shape: Shape) -> Self {
        let bytes: Vec<u8> = values.iter().flat_map(|&v| v.to_bits().to_le_bytes()).collect();
        Self { data: bytes, shape, data_type: DataType::Float32, device: TensorDevice::CPU }
    }

    /// Get a float value at an index
    pub fn get_f32(&self, idx: usize) -> f32 {
        assert_eq!(self.data_type, DataType::Float32);
        let bytes = &self.data[idx * 4..(idx + 1) * 4];
        f32::from_le_bytes(bytes.try_into().unwrap())
    }

    /// Set a float value at an index
    pub fn set_f32(&mut self, idx: usize, value: f32) {
        assert_eq!(self.data_type, DataType::Float32);
        let bytes = value.to_le_bytes();
        self.data[idx * 4..(idx + 1) * 4].copy_from_slice(&bytes);
    }

    /// Get a reference to the raw data
    pub fn data(&self) -> &[u8] { &self.data }

    /// Get a mutable reference to the raw data
    pub fn data_mut(&mut self) -> &mut [u8] { &mut self.data }
}

/// Device where tensor data lives
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TensorDevice {
    CPU,
    GPU,
    NPU,
    DSP,
}

/// Inference result
#[derive(Debug)]
pub struct InferenceResult {
    pub outputs: Vec<Tensor>,
    pub latency_ms: f32,
    pub backend_used: BackendKind,
}

impl InferenceResult {
    pub fn new(outputs: Vec<Tensor>, latency_ms: f32, backend_used: BackendKind) -> Self {
        Self { outputs, latency_ms, backend_used }
    }
}
