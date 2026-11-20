#pragma once
// OPTIONAL. You don't need this file: on first start DeskBot opens the Wi-Fi
// "DeskBot-Setup", where you choose your Wi-Fi and enter the PC address.
//
// If you copy this file to secrets.h, these values are used as defaults until
// something is saved in the setup page. secrets.h is git-ignored.

#define WIFI_SSID      "dot"
#define WIFI_PASSWORD  "88888888"

// PC running the DeskBot server (app.py or DeskBot.exe)
#define SERVER_URL     "http://192.168.0.111:8000"
