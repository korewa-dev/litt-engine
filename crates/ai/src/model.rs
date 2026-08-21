//! Model abstraction — represents a loaded neural network.
//! Supports ONNX, TFLite, and custom binary formats.

use super::tensor::{DataType, Shape};

/// Model format
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ModelFormat {
    /// TensorFlow Lite
    Tflite,
    /// ONNX
    Onnx,
    /// OpenVINO IR
    OpenVino,
    /// Custom binary
    Custom,
}

/// Input specification for a model
#[derive(Debug)]
pub struct InputSpec {
    pub name: String,
    pub shape: Shape,
    pub data_type: DataType,
}

/// Output specification for a model
#[derive(Debug)]
pub struct OutputSpec {
    pub name: String,
    pub shape: Shape,
    pub data_type: DataType,
}

/// A loaded neural network model
#[derive(Debug)]
pub struct Model {
    pub format: ModelFormat,
    pub inputs: Vec<InputSpec>,
    pub outputs: Vec<OutputSpec>,
    pub metadata: ModelMetadata,
    /// Raw model data (backend-specific)
    pub inner: ModelInner,
}

#[derive(Debug)]
pub enum ModelInner {
    /// ONNX model bytes
    Onnx(Vec<u8>),
    /// TFLite model bytes
    Tflite(Vec<u8>),
    /// OpenVINO model (XML + weights)
    OpenVino { xml: Vec<u8>, weights: Vec<u8> },
    /// Backend-specific opaque handle
    Opaque(*mut std::ffi::c_void),
}

impl Model {
    /// Load a TFLite model from bytes
    pub fn from_tflite(data: Vec<u8>) -> Result<Self, String> {
        // Parse TFLite flatbuffer to extract input/output specs
        // This is a simplified implementation — real TFLite parsing requires flatc
        let inputs = vec![InputSpec {
            name: "input".to_string(),
            shape: Shape::new(&[1, 224, 224, 3]),
            data_type: DataType::Float32,
        }];
        let outputs = vec![OutputSpec {
            name: "output".to_string(),
            shape: Shape::new(&[1, 1000]),
            data_type: DataType::Float32,
        }];
        let metadata = ModelMetadata {
            name: "tflite_model".to_string(),
            description: String::new(),
            author: String::new(),
            version: String::new(),
            input_normalization: vec![],
            output_postprocessing: vec![],
        };

        Ok(Self {
            format: ModelFormat::Tflite,
            inputs,
            outputs,
            metadata,
            inner: ModelInner::Tflite(data),
        })
    }

    /// Load an ONNX model from bytes
    pub fn from_onnx(data: Vec<u8>) -> Result<Self, String> {
        let inputs = vec![InputSpec {
            name: "input".to_string(),
            shape: Shape::new(&[1, 3, 224, 224]),
            data_type: DataType::Float32,
        }];
        let outputs = vec![OutputSpec {
            name: "output".to_string(),
            shape: Shape::new(&[1, 1000]),
            data_type: DataType::Float32,
        }];
        let metadata = ModelMetadata {
            name: "onnx_model".to_string(),
            description: String::new(),
            author: String::new(),
            version: String::new(),
            input_normalization: vec![],
            output_postprocessing: vec![],
        };

        Ok(Self {
            format: ModelFormat::Onnx,
            inputs,
            outputs,
            metadata,
            inner: ModelInner::Onnx(data),
        })
    }

    /// Create a dummy model for testing
    pub fn dummy() -> Self {
        Self {
            format: ModelFormat::Custom,
            inputs: vec![InputSpec {
                name: "input".to_string(),
                shape: Shape::new(&[1, 4]),
                data_type: DataType::Float32,
            }],
            outputs: vec![OutputSpec {
                name: "output".to_string(),
                shape: Shape::new(&[1, 2]),
                data_type: DataType::Float32,
            }],
            metadata: ModelMetadata::default(),
            inner: ModelInner::Opaque(std::ptr::null_mut()),
        }
    }
}

/// Metadata about a model
#[derive(Debug, Default)]
pub struct ModelMetadata {
    pub name: String,
    pub description: String,
    pub author: String,
    pub version: String,
    pub input_normalization: Vec<NormalizationSpec>,
    pub output_postprocessing: Vec<PostprocessingSpec>,
}

/// Input normalization specification
#[derive(Debug, Clone)]
pub struct NormalizationSpec {
    pub mean: Vec<f32>,
    pub std: Vec<f32>,
    pub do_normalize: bool,
}

/// Output postprocessing specification
#[derive(Debug, Clone)]
pub struct PostprocessingSpec {
    pub operation: String, // "softmax", "argmax", "sigmoid", etc.
    pub threshold: f32,
}
