use crate::io::yaml::read_yaml_file;
use crate::model::rsrc::rsrc_type::ResourceType;
use crate::model::rsrc::static_resource::StaticResource;
use anyhow::{Context, Result};
use serde::Deserialize;
use std::path::Path;

#[derive(Debug, Deserialize)]
struct ResourcesYaml {
  #[serde(default)]
  resource_types: Vec<ResourceType>,
  #[serde(default)]
  resources: Vec<StaticResource>,
}

#[derive(Debug)]
pub struct ResourcesBundle {
  pub resource_types: Vec<ResourceType>,
  pub resources: Vec<StaticResource>,
}

pub fn read_resources(path: &Path) -> Result<ResourcesBundle> {
  let resources: ResourcesYaml = read_yaml_file(path)
    .with_context(|| format!("failed to load resources yaml at {}", path.display()))?;
  Ok(ResourcesBundle {
    resource_types: resources.resource_types,
    resources: resources.resources,
  })
}

#[cfg(test)]
mod tests {
  use super::read_resources;
  use crate::model::rsrc::rsrc_type::ResourceType;
  use crate::model::rsrc::static_resource::StaticResource;
  use serde_json::json;
  use std::env;
  use std::fs;
  use std::path::PathBuf;
  use std::time::{SystemTime, UNIX_EPOCH};

  fn write_temp_resources_yaml(contents: &str) -> PathBuf {
    let nanos = SystemTime::now()
      .duration_since(UNIX_EPOCH)
      .expect("clock went backwards")
      .as_nanos();
    let path = env::temp_dir().join(format!("resources-{}.yaml", nanos));
    fs::write(&path, contents).expect("write temp yaml");
    path
  }

  #[test]
  fn read_resources_includes_custom_resources() {
    let yaml = r#"
resource_types:
  - id: "fep"
    description: "front-end processor"
    attributes_schema:
      bands: []
resources:
  - id: "fep-1"
    kind: "fep"
    capacity: 1
    attributes:
      bands: ["S", "X"]
  - id: "pool-a"
    kind: "fep_pool"
    capacity: 3
    attributes:
      region: "west"
"#;
    let path = write_temp_resources_yaml(yaml);
    let result = read_resources(&path);
    fs::remove_file(&path).expect("cleanup temp yaml");

    let bundle = result.expect("read resources");
    let expected_types = vec![ResourceType {
      id: "fep".to_string(),
      description: Some("front-end processor".to_string()),
      attributes_schema: json!({ "bands": [] }),
    }];
    let expected = vec![
      StaticResource {
        id: "fep-1".to_string(),
        kind: "fep".to_string(),
        capacity: 1,
        attributes: json!({ "bands": ["S", "X"] }),
      },
      StaticResource {
        id: "pool-a".to_string(),
        kind: "fep_pool".to_string(),
        capacity: 3,
        attributes: json!({ "region": "west" }),
      },
    ];

    assert_eq!(bundle.resource_types, expected_types);
    assert_eq!(bundle.resources, expected);
  }

  #[test]
  fn read_resources_supports_antenna_kind() {
    let yaml = r#"
resources:
  - id: "ant-1"
    kind: "antenna"
    capacity: 1
    attributes:
      network: "net-a"
      latitude: 34.5
      longitude: -120.7
      altitude: 12.3
      capabilities:
        - band: "S"
          tracking_modes: ["L1", "L2"]
      frequency_ranges:
        S:
          uplink_MHz: [2025, 2110]
          downlink_MHz: [2200, 2290]
      constraints:
        - name: "max_elevation"
          parameters:
            min: 10.0
            max: 85.0
"#;
    let path = write_temp_resources_yaml(yaml);
    let result = read_resources(&path);
    fs::remove_file(&path).expect("cleanup temp yaml");

    let bundle = result.expect("read resources");
    let expected = vec![StaticResource {
      id: "ant-1".to_string(),
      kind: "antenna".to_string(),
      capacity: 1,
      attributes: json!({
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
      }),
    }];

    assert_eq!(bundle.resources, expected);
  }
}
