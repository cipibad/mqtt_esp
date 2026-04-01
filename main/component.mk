ifdef CONFIG_NORTH_INTERFACE_MQTT
COMPONENT_EMBED_TXTFILES += cert_bundle.pem
endif

ifdef CONFIG_NORTH_INTERFACE_HTTP
COMPONENT_EMBED_TXTFILES += http/index.css http/index.html http/index.js
endif
