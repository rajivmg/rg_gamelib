reproject latlongToCubemap %1.hdr %1_hc.hdr 512 640
texassemble.exe cube-from-hc -y -o %1.dds %1_hc.hdr