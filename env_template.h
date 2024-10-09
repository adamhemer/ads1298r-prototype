// READ BEFORE CHANGING:
// This is a template for the "env.h" file. Please follow the steps below.
// 1. Rename this file to env.h BEFORE making any changes. If you don't change the name your credentials could be backed up to git.
// 2. Replace the network credentials with the networks you wish to use the device on.
//      2.1. The enterprise flag, username, and identity are only necessary for eduroam. Any private network will not require these.
// 3. Update the Host IP and optionally the Port for each network by connecting the computer you are running MATLAB on and checking its IP
//      3.1. This IP may change (especially for public networks) so you may need to update this fairly frequently
// 4. Ensure that allNetworks[] contains all the networks you wish to connect to
// 5. Update numNetworks to how many networks you have specified.

#include "WiFiCredentials.h"

// Add as many networks as necessary:    Host IP            Port     SSID / Name             Password      [Enterprise?  Username    Identity]
WiFiCredentials homeNetwork         = { "192.168.0.101",    4500,   "Telstra123ABC",        "homenetpass",   false };
WiFiCredentials hotspot             = { "192.168.121.65",   4500,   "GalaxyHotspot",        "hotspotpass",   false };
WiFiCredentials eduroam             = { "10.30.7.30",       4500,   "eduroam",              "eduroampass",   true,       "fann1234", "fann1234@flinders.edu.au" };

WiFiCredentials allNetworks[] = { homeNetwork, hotspot, eduroam };
int numNetworks = 3;