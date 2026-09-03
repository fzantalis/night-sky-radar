#include "TleFetcher.h"
#include "TleStore.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

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
// The brief is explicit that a TLS failure must be reported, not worked
// around with setInsecure(). This is real certificate verification, just
// pinned to one root instead of a whole bundle: the self-signed root that
// signs celestrak.org's chain, "Sectigo Public Server Authentication Root
// R46" (Mozilla-trusted, expires 2046-03-21). Verified locally with
// `openssl s_client -connect celestrak.org:443 -showcerts` plus
// `openssl verify -CAfile <this root> -untrusted <intermediate R36> <leaf>`,
// which returned OK. WiFiClientSecure::setCACert() performs the same mbedtls
// chain validation setCACertBundle() would have, against this one anchor.
constexpr char kCelestrakRootCA[] = R"PEM(-----BEGIN CERTIFICATE-----
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
)PEM";

}  // namespace

namespace tlefetcher {

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
        client.setCACert(kCelestrakRootCA);
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

}  // namespace tlefetcher

int parseTleText(const String& raw, std::vector<Tle>& out, int maxCount) {
    out.clear();

    const int n = static_cast<int>(raw.length());
    String triple[3];
    int held = 0;
    int i = 0;

    // Walk the buffer one line at a time, holding at most three lines. Nothing
    // larger than a single element set is ever resident, which is what makes the
    // 1.2 MB Starlink group tractable at M3.
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
