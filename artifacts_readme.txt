Installation instructions:

1. Install a Remix release from https://github.com/NVIDIAGameWorks/rtx-remix
(Generally, the last release that came out before the build you are installing)

2. Copy and paste the d3d9.dll from this package into the PARENT directory of
the `.trex/` folder from that release, and the NvRemixBridge.exe file into the
`.trex/` folder itself.

Notes:

- You should be prompted to overwrite existing files - if you aren't, you're
probably putting the files in the wrong place.

- Make sure you are overwriting the correct d3d9.dll file, because there are
two of them, but the one outside the `.trex/` folder is the much smaller one
and built by the Bridge project. DO NOT OVERWRITE the d3d9.dll file inside
`.trex/`, because that one is from DXVK-Remix. You can also check the Details
page of the file properties dialog, which will identify the d3d9.dll as either
bridge-remix or dxvk-remix.