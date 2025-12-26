use scheduler_core::extensions::api::StaticResourceDescriptor;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::HashMap;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct FrequencyRange {
    #[serde(rename = "uplink_MHz", alias = "uplink_mhz")]
    #[serde(default)]
    pub uplink_mhz: Option<Vec<u32>>,
    #[serde(rename = "downlink_MHz", alias = "downlink_mhz")]
    #[serde(default)]
    pub downlink_mhz: Option<Vec<u32>>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AntennaCapability {
    pub band: String,
    pub tracking_modes: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AntennaConstraint {
    pub name: String,
    pub parameters: HashMap<String, f64>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Antenna {
    pub id: String,
    pub network: String,
    pub latitude: f64,
    pub longitude: f64,
    pub altitude: f64,
    pub capabilities: Vec<AntennaCapability>,
    #[serde(default)]
    pub frequency_ranges: HashMap<String, FrequencyRange>,
    pub constraints: Vec<AntennaConstraint>,
}

impl Antenna {
    pub fn from_resource(resource: &StaticResourceDescriptor) -> Result<Self, String> {
        if resource.kind.as_str() != "antenna" {
            return Err("not an antenna resource".to_string());
        }
        let mut attributes: Value = serde_json::from_str(resource.attributes_json.as_str())
            .map_err(|err| err.to_string())?;
        if let Value::Object(ref mut object) = attributes {
            object.insert(
                "id".to_string(),
                Value::String(resource.id.as_str().to_string()),
            );
        } else {
            return Err("antenna attributes must be an object".to_string());
        }
        serde_json::from_value(attributes).map_err(|err| err.to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::Antenna;
    use abi_stable::std_types::RString;
    use scheduler_core::extensions::api::StaticResourceDescriptor;
    use serde_json::json;
    use std::collections::HashMap;

    #[test]
    fn antenna_from_resource_parses_attributes() {
        let resource = StaticResourceDescriptor {
            id: RString::from("ant-1"),
            kind: RString::from("antenna"),
            capacity: 1,
            attributes_json: RString::from(json!({
                "network": "net-a",
                "latitude": 34.5,
                "longitude": -120.7,
                "altitude": 12.3,
                "capabilities": [
                    {
                        "band": "S",
                        "tracking_modes": ["L1", "L2"]
                    }
                ],
                "frequency_ranges": {
                    "S": {
                        "uplink_MHz": [2025, 2110],
                        "downlink_MHz": [2200, 2290]
                    }
                },
                "constraints": [
                    {
                        "name": "max_elevation",
                        "parameters": {
                            "min": 10.0,
                            "max": 85.0
                        }
                    }
                ]
            }).to_string()),
        };

        let antenna = Antenna::from_resource(&resource).expect("parse antenna");
        assert_eq!(antenna.id, "ant-1");
        assert_eq!(antenna.network, "net-a");
        assert_eq!(antenna.frequency_ranges.len(), 1);
        assert_eq!(antenna.capabilities.len(), 1);
        assert_eq!(antenna.constraints.len(), 1);
        let params = &antenna.constraints[0].parameters;
        let mut expected = HashMap::new();
        expected.insert("min".to_string(), 10.0);
        expected.insert("max".to_string(), 85.0);
        assert_eq!(params, &expected);
    }
}
