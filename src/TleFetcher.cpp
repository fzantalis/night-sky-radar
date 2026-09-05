#include "TleFetcher.h"
#include "TleStore.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <cstring>

namespace {

// Deviation from the task-9 brief's Step 2/3, recorded here for the record:
//
// Neither `x509_crt_imported_bundle_bin_start` nor `x509_crt_bundle_start`
// exists in this toolchain (framework-arduinoespressif32 3.20007.0, i.e.
// arduino-esp32 2.0.7, via platform espressif32 6.1.0). Both are undefined
// references at COMPILE time, not link time - grepping the whole framework
// and platform package trees for any generated cert-bundle object or the
// python step that produces one turns up nothing. `WiFiClientSecure`'s
// `setCACertBundle()` exists, but arduino-esp32's PlatformIO packaging never
// runs the ESP-IDF/menuconfig CMake step that embeds a default trust bundle
// and emits those symbols, so there is no default bundle to point at on this
// toolchain - not a naming difference, an actually-absent artifact.
//
// The project's global TLS constraint forbids both setInsecure() AND pinning
// a single root (a single pin bricks fetching the day the CA rotates - the
// exact failure mode CelesTrak's own history of switching CAs makes plausible
// for a device meant to run unattended for years). So this is a small,
// concatenated multi-root trust store, not a bundle and not a single pin.
// `WiFiClientSecure::setCACert()` calls `mbedtls_x509_crt_parse()`, which
// accepts multiple concatenated PEM certificates and adds every one of them
// to the trust store - confirmed against ssl_client.cpp in this framework.
// Five self-signed roots below cover the overwhelming majority of what a
// site like CelesTrak would plausibly rotate to: Sectigo's current root
// (the one actually signing celestrak.org today), Sectigo's long-standing
// legacy root, Let's Encrypt (the single most likely fallback for a small
// public site), and the two most common commercial/hosting roots. Each was
// pulled from curl.se's Mozilla-derived CA bundle (the canonical multi-CA
// distribution point, not typed from memory) and independently verified
// self-signed/unexpired with `openssl x509 -noout -subject -issuer -dates`
// before being embedded; see task-9-report.md for the fingerprints and the
// `openssl verify` / `crl2pkcs7` sanity checks run against this exact blob.
constexpr char kTrustedRootsPem[] = R"PEM(-----BEGIN CERTIFICATE-----
MIIFijCCA3KgAwIBAgIQdY39i658BwD6qSWn4cetFDANBgkqhkiG9w0BAQwFADBf
MQswCQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQD
Ey1TZWN0aWdvIFB1YmxpYyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBSNDYw
HhcNMjEwMzIyMDAwMDAwWhcNNDYwMzIxMjM1OTU5WjBfMQswCQYDVQQGEwJHQjEY
MBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1TZWN0aWdvIFB1Ymxp
YyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBSNDYwggIiMA0GCSqGSIb3DQEB
AQUAA4ICDwAwggIKAoICAQCTvtU2UnXYASOgHEdCSe5jtrch/cSV1UgrJnwUUxDa
ef0rty2k1Cz66jLdScK5vQ9IPXtamFSvnl0xdE8H/FAh3aTPaE8bEmNtJZlMKpnz
SDBh+oF8HqcIStw+KxwfGExxqjWMrfhu6DtK2eWUAtaJhBOqbchPM8xQljeSM9xf
iOefVNlI8JhD1mb9nxc4Q8UBUQvX4yMPFF1bFOdLvt30yNoDN9HWOaEhUTCDsG3X
ME6WW5HwcCSrv0WBZEMNvSE6Lzzpng3LILVCJ8zab5vuZDCQOc2TZYEhMbUjUDM3
IuM47fgxMMxF/mL50V0yeUKH32rMVhlATc6qu/m1dkmU8Sf4kaWD5QazYw6A3OAS
VYCmO2a0OYctyPDQ0RTp5A1NDvZdV3LFOxxHVp3i1fuBYYzMTYCQNFu31xR13NgE
SJ/AwSiItOkcyqex8Va3e0lMWeUgFaiEAin6OJRpmkkGj80feRQXEgyDet4fsZfu
+Zd4KKTIRJLpfSYFplhym3kT2BFfrsU4YjRosoYwjviQYZ4ybPUHNs2iTG7sijbt
8uaZFURww3y8nDnAtOFr94MlI1fZEoDlSfB1D++N6xybVCi0ITz8fAr/73trdf+L
HaAZBav6+CuBQug4urv7qv094PPK306Xlynt8xhW6aWWrL3DkJiy4Pmi1KZHQ3xt
zwIDAQABo0IwQDAdBgNVHQ4EFgQUVnNYZJX5khqwEioEYnmhQBWIIUkwDgYDVR0P
AQH/BAQDAgGGMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAC9c
mTz8Bl6MlC5w6tIyMY208FHVvArzZJ8HXtXBc2hkeqK5Duj5XYUtqDdFqij0lgVQ
YKlJfp/imTYpE0RHap1VIDzYm/EDMrraQKFz6oOht0SmDpkBm+S8f74TlH7Kph52
gDY9hAaLMyZlbcp+nv4fjFg4exqDsQ+8FxG75gbMY/qB8oFM2gsQa6H61SilzwZA
Fv97fRheORKkU55+MkIQpiGRqRxOF3yEvJ+M0ejf5lG5Nkc/kLnHvALcWxxPDkjB
JYOcCj+esQMzEhonrPcibCTRAUH4WAP+JWgiH5paPHxsnnVI84HxZmduTILA7rpX
DhjvLpr3Etiga+kFpaHpaPi8TD8SHkXoUsCjvxInebnMMTzD9joiFgOgyY9mpFui
TdaBJQbpdqQACj7LzTWb4OE4y2BThihCQRxEV+ioratF4yUQvNs+ZUH7G6aXD+u5
dHn5HrwdVw1Hr8Mvn4dGp+smWg9WY7ViYG4A++MnESLn/pmPNPW56MORcr3Ywx65
LvKRRFHQV80MNNVIIb/bE/FmJUNS0nAiNs2fxBx1IK1jcmMGDw4nztJqDby1ORrp
0XZ60Vzk50lJLVU3aPAaOpg+VBeHVOmmJ1CJeyAvP/+/oYtKR5j/K3tJPsMpRmAY
QqszKbrAKbkTidOIijlBO8n9pu0f9GBj39ItVQGL
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
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
Q+6IHdfGjjxDah2nGN59PRbxYvnKkKj9
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAwTzELMAkGA1UE
BhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2VhcmNoIEdyb3VwMRUwEwYDVQQD
EwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQG
EwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMT
DElTUkcgUm9vdCBYMTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54r
Vygch77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+0TM8ukj1
3Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6UA5/TR5d8mUgjU+g4rk8K
b4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sWT8KOEUt+zwvo/7V3LvSye0rgTBIlDHCN
Aymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyHB5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ
4Q7e2RCOFvu396j3x+UCB5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf
1b0SHzUvKBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWnOlFu
hjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTnjh8BCNAw1FtxNrQH
usEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbwqHyGO0aoSCqI3Haadr8faqU9GY/r
OPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CIrU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4G
A1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY
9umbbjANBgkqhkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ3BebYhtF8GaV
0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KKNFtY2PwByVS5uCbMiogziUwt
hDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJw
TdwJx4nLCgdNbOhdjsnvzqvHu7UrTkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nx
e5AW0wdeRlN8NwdCjNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZA
JzVcoyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq4RgqsahD
YVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPAmRGunUHBcnWEvgJBQl9n
JEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57demyPxgcYxn/eR44/KJ4EBs+lVDR3veyJ
m+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBhMQswCQYDVQQG
EwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSAw
HgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUx
MjAwMDBaMGExCzAJBgNVBAYTAlVTMRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3
dy5kaWdpY2VydC5jb20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkq
hkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI2/Ou8jqJ
kTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx1x7e/dfgy5SDN67sH0NO
3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQq2EGnI/yuum06ZIya7XzV+hdG82MHauV
BJVJ8zUtluNJbd134/tJS7SsVQepj5WztCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyM
UNGPHgm+F6HmIcr9g+UQvIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQAB
o0IwQDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV5uNu
5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY1Yl9PMWLSn/pvtsr
F9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4NeF22d+mQrvHRAiGfzZ0JFrabA0U
WTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NGFdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBH
QRFXGU7Aj64GxJUTFy8bJZ918rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/
iyK5S9kJRaTepLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsFADA5MQswCQYD
VQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6b24gUm9vdCBDQSAxMB4XDTE1
MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTELMAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpv
bjEZMBcGA1UEAxMQQW1hem9uIFJvb3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoC
ggEBALJ4gHHKeNXjca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgH
FzZM9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qwIFAGbHrQ
gLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6VOujw5H5SNz/0egwLX0t
dHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L93FcXmn/6pUCyziKrlA4b9v7LWIbxcce
VOF34GfID5yHI9Y/QCB/IIDEgEw+OyQmjgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB
/zAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3
DQEBCwUAA4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDIU5PM
CCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUsN+gDS63pYaACbvXy
8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vvo/ufQJVtMVT8QtPHRh8jrdkPSHCa
2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2
xJNDd2ZhwLnoQdeXeGADbkpyrqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)PEM";

}  // namespace

namespace tlefetcher {

const char* trustedRootsPem() { return kTrustedRootsPem; }

bool fetchGroup(const char* group, String& outRaw) {
    outRaw = String();

    String url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=";
    url += group;
    url += "&FORMAT=tle";

    bool ok = false;

    // Scoped so the TLS client and its ~40-50 KB of internal heap are released
    // the moment we are done, before anything else allocates.
    {
        WiFiClientSecure client;
        client.setCACert(kTrustedRootsPem);
        client.setTimeout(15000);

        HTTPClient http;
        http.setConnectTimeout(15000);
        http.setTimeout(15000);
        http.setUserAgent("esp32-sky-radar/0.1");

        if (!http.begin(client, url)) {
            Serial.println("[tle] http begin failed");
            return false;
        }

        const int code = http.GET();
        if (code == HTTP_CODE_OK) {
            outRaw = http.getString();
            ok = outRaw.length() > 0;
        } else {
            Serial.printf("[tle] GET failed, code %d\n", code);
        }

        http.end();
    }

    if (!ok) return false;

    Serial.printf("[tle] fetched %u bytes for group %s\n",
                  static_cast<unsigned>(outRaw.length()), group);
    return true;
}

// Streams a CelesTrak group without ever holding the whole body. See the
// declaration in TleFetcher.h for the full contract.
int fetchGroupStreaming(const char* group, TleFilter keep, std::vector<Tle>& out,
                        int maxKeep, int* totalParsed, uint32_t* minFreeHeapOut) {
    out.clear();
    if (totalParsed) *totalParsed = 0;

    uint32_t minFreeHeap = ESP.getFreeHeap();
    auto trackHeap = [&]() {
        const uint32_t free = ESP.getFreeHeap();
        if (free < minFreeHeap) minFreeHeap = free;
    };

    String url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=";
    url += group;
    url += "&FORMAT=tle";

    int accepted = 0;
    int seenTriples = 0;

    {
        WiFiClientSecure client;
        client.setCACert(kTrustedRootsPem);
        client.setTimeout(15000);

        HTTPClient http;
        http.setConnectTimeout(15000);
        http.setTimeout(15000);
        http.setUserAgent("esp32-sky-radar/0.1");

        if (!http.begin(client, url)) {
            Serial.println("[tle] streaming http begin failed");
            return -1;
        }

        const int code = http.GET();
        if (code != HTTP_CODE_OK) {
            Serial.printf("[tle] streaming GET failed, code %d\n", code);
            http.end();
            return -1;
        }

        trackHeap();

        WiFiClient* stream = http.getStreamPtr();
        const long contentLen = http.getSize();   // -1 if unknown/chunked

        // Bounded per-line buffer: a TLE line is 69 columns, a name at most
        // 24. 160 bytes is generous headroom without ever approaching the
        // ~1.7 MB the whole body would take. Nothing larger than one line is
        // ever resident, and nothing larger than one three-line element set
        // (triple[3]) is ever held across iterations.
        constexpr size_t kLineBufSize = 160;
        char lineBuf[kLineBufSize];
        char triple[3][kLineBufSize];
        int held = 0;
        long totalRead = 0;

        for (;;) {
            if (contentLen >= 0 && totalRead >= contentLen) break;   // consumed the declared body
            if (!http.connected() && !stream->available()) break;    // socket closed, nothing buffered

            const int n = static_cast<int>(stream->readBytesUntil('\n', lineBuf, kLineBufSize - 1));
            if (n <= 0) {
                if (!http.connected()) break;   // genuinely done, not just a brief stall
                continue;
            }
            totalRead += static_cast<long>(n) + 1;
            lineBuf[n] = '\0';

            // Trim a trailing '\r' and any padding, same as parseTleText().
            int end = n;
            while (end > 0 && (lineBuf[end - 1] == '\r' || lineBuf[end - 1] == ' ')) end--;
            lineBuf[end] = '\0';
            int start = 0;
            while (lineBuf[start] == ' ') start++;
            if (start >= end) { trackHeap(); continue; }   // blank line between triples

            std::strncpy(triple[held], lineBuf + start, kLineBufSize - 1);
            triple[held][kLineBufSize - 1] = '\0';
            held++;
            if (held < 3) { trackHeap(); continue; }

            held = 0;
            Tle t;
            if (parseTle(triple[0], triple[1], triple[2], t)) {
                seenTriples++;
                if (keep(t) && static_cast<int>(out.size()) < maxKeep) {
                    out.push_back(t);
                    accepted++;
                }
            } else {
                Serial.printf("[tle] streaming skipped malformed entry: %s\n", triple[0]);
            }
            trackHeap();
        }

        http.end();
    }

    if (totalParsed) *totalParsed = seenTriples;
    if (minFreeHeapOut) *minFreeHeapOut = minFreeHeap;

    Serial.printf("[tle] streaming fetch of '%s': %d triple(s) parsed, %d kept, "
                  "min free heap %u bytes\n",
                  group, seenTriples, accepted, static_cast<unsigned>(minFreeHeap));

    return accepted;
}

}  // namespace tlefetcher

int parseTleText(const String& raw, std::vector<Tle>& out, int maxCount) {
    out.clear();

    const int n = static_cast<int>(raw.length());
    String triple[3];
    int held = 0;
    int i = 0;

    // Walk the buffer one line at a time, holding at most three lines. This
    // does NOT make the whole body cheap - `raw` is already fully resident by
    // the time this runs, since fetchGroup() (the only caller for `stations`/
    // `visual`) reads the whole response into one Arduino String first. That
    // is fine for those two groups (~4 KB / ~22 KB) but is exactly why the
    // ~1.7 MB `starlink` group needs fetchGroupStreaming() instead, which
    // never materialises the body at all. A previous comment here claimed
    // this function was what made Starlink tractable; it was not - a
    // reviewer flagged it during M0-M1, and it is corrected here at M3.
    while (i < n && static_cast<int>(out.size()) < maxCount) {
        int nl = raw.indexOf('\n', i);
        if (nl < 0) nl = n;

        String line = raw.substring(i, nl);
        i = nl + 1;

        line.replace("\r", "");
        line.trim();
        if (line.length() == 0) continue;

        triple[held++] = line;
        if (held < 3) continue;

        held = 0;
        Tle t;
        if (parseTle(triple[0].c_str(), triple[1].c_str(), triple[2].c_str(), t)) {
            out.push_back(t);
        } else {
            Serial.printf("[tle] skipped malformed entry: %s\n", triple[0].c_str());
        }
    }

    return static_cast<int>(out.size());
}
