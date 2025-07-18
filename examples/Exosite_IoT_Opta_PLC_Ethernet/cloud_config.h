// Fill in the domain of your IoT Connector
#define CONNECTOR_DOMAIN "a5c9szkndm4g00000.m2.exosite.io"

// Ref: https://docs.exosite.io/schema/channel-signal_io_schema/#channel-configuration-config_io
const char CHANNEL_CONFIG[] = R"json(
{
  "channels": {
    "001": {
      "display_name": "Input 1",
      "properties": {
        "data_type": "BOOLEAN"
      },
      "protocol_config": {
        "report_rate": 10000
      }
    },
    "002": {
      "display_name": "Input 2",
      "properties": {
        "data_type": "BOOLEAN"
      },
      "protocol_config": {
        "report_rate": 10000
      }
    },
    "003": {
      "display_name": "Input 3",
      "properties": {
        "data_type": "BOOLEAN"
      },
      "protocol_config": {
        "report_rate": 10000
      }
    },
    "004": {
      "display_name": "Input 4",
      "properties": {
        "data_type": "BOOLEAN"
      },
      "protocol_config": {
        "report_rate": 10000
      }
    },
    "005": {
      "display_name": "Potentiometer",
      "properties": {
        "data_type": "ELEC_POTENTIAL",
        "data_unit": "VOLT",
        "precision": 2
      },
      "protocol_config": {
        "report_rate": 10000
      }
    }
  }
}
)json";
