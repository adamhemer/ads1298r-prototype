struct WiFiCredentials {
    // Standard Network
    char* ssid;
    char* password;

    // For eduroam
    bool enterprise;
    char* username;
    char* identity;

};