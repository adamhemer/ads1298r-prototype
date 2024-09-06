struct WiFiCredentials {
    // Server
    char* host;
    uint16_t port;

    // Standard Network
    char* ssid;
    char* password;

    // For eduroam
    bool enterprise;
    char* username;
    char* identity;
};