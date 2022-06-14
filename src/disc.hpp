#pragma once

#include "types.hpp"
#include "crc.hpp"
#include "msf.hpp"

#include <cstdio>
#include <string.h>
#include <optional>

/// Size of a CD sector in bytes
static constexpr u32 SECTOR_SIZE = 2352;

/// CD-ROM sector sync pattern: 10 0xff surrounded by two 0x00. Not
/// used in CD-DA audio tracks.
static constexpr u8 SECTOR_SYNC_PATTERN[12] = {
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

struct XaSector {
  u8 raw[SECTOR_SIZE];

  XaSector() {
    memset(raw, 0, sizeof(u8) * SECTOR_SIZE);
  }

  constexpr u8 data_byte(u16 idx) {
    u32 index = static_cast<u32>(idx);
    return raw[index];
  }

  std::optional<XaSector> validate_mode_1_2(MSF msf) {
    if(memcmp(raw, SECTOR_SYNC_PATTERN, 12) != 0) {
      LOG_ERROR("invalid sector sync at %d %d %d", msf.m, msf.s, msf.f);
      return std::nullopt;
    }

    MSF our = this->msf();
    if (our != msf) {
      LOG_ERROR("unexpected sector MSF: expected");
      our.print();
      printf("got ");
      msf.print();
      return std::nullopt;
    }

    u8 mode = raw[15];

    switch (mode) {
    case 1:
      LOG_ERROR("Unhandled mode 1 sector at %d %d %d", msf.m, msf.s, msf.f);
      return std::nullopt;

    case 2:
      return validate_mode2();

    default:
      LOG_ERROR("Unhandled sector mode %d at %d %d %d", mode, msf.m, msf.s,
                msf.f);
      return std::nullopt;
    }
  }

  std::optional<XaSector> validate_mode2() {
    u8 submode = raw[18];
    u8 submode_copy = raw[22];

    if (submode != submode_copy) {
      LOG_ERROR("Sector ");
      msf().print();
      printf(" mode 2 submode missmatch %02x, %02x", submode, submode_copy);
      return std::nullopt;
    }

    if ((submode & 0x20) != 0) {
      return validate_mode2_form2();
    } else {
      return validate_mode2_form1();
    }
  }

  std::optional<XaSector> validate_mode2_form1() {
    u32 crc = crc32(raw+16, 2072-16);

    u32 sector_crc = static_cast<u32>(raw[2072]) |
                     (static_cast<u32>(raw[2073]) << 8) |
                     (static_cast<u32>(raw[2074]) << 16) |
                     (static_cast<u32>(raw[2075]) << 24);

    if (crc != sector_crc) {
      MSF m = msf();
      LOG_ERROR("Sector %d %d %d: Mode 2 Form 1 CRC missmatch", m.m, m.s, m.f);
      return std::nullopt;
    }

    return *this;
  }

  std::optional<XaSector> validate_mode2_form2() {
    return *this;
  }

  u8* data_bytes() { return raw; }

  MSF msf() {
    MSF m;
    from_bcd(m, raw[12], raw[13], raw[14]);
    return m;
  }
};

enum Region {
  japan,
  north_america,
  europe,
};

struct Disc {
  FILE *file;
  Region region;

  std::optional<XaSector> read_sector(MSF msf) {
    u32 idx = msf.sector_index() - 150;

    u64 pos = static_cast<u64>(idx) * SECTOR_SIZE;

    fseek(file, pos, SEEK_SET);

    XaSector sector = XaSector();
    u64 nread = 0;

    while(nread < SECTOR_SIZE) {
      size_t read =
          fread(sector.raw + nread, sizeof(u8), SECTOR_SIZE - nread, file);
      if (read <= 0) {
        LOG_ERROR("Short sector read");
        return std::nullopt;
      }
      nread += read;
    }

    return sector;
  }

  std::optional<XaSector> read_data_sector(MSF msf) {
    auto sector = read_sector(msf);
    if(sector) {
      return sector->validate_mode_1_2(msf);
    }

    return std::nullopt;
  }

  std::optional<Disc> extract_region() {
    MSF msf;
    if(from_bcd(msf, 0x00, 0x02, 0x04)) {
      LOG_ERROR("Bad MSF");
      return std::nullopt;
    }

    auto sector = read_data_sector(msf);
    if(!sector) {
      return std::nullopt;
    }

    const int blob_size = 100-24;
    char license_blob[blob_size + 1];
    memcpy(license_blob, sector->data_bytes() + 24, blob_size);
    license_blob[blob_size] = 0;

    char cleaned_blob[blob_size + 1];
    size_t last = 0;
    for(size_t i = 0; i < blob_size; ++i) {
      if(license_blob[i] >= 'A' && license_blob[i] <= 'Z') {
	cleaned_blob[last++] = license_blob[i];
      } else if (license_blob[i] >= 'a' && license_blob[i] <= 'z') {
        cleaned_blob[last++] = license_blob[i];
      }
    }
    cleaned_blob[last] = 0;

    if(strcmp(cleaned_blob, "LicensedbySonyComputerEntertainmentInc") == 0) {
      region = Region::japan;
    } else if (strcmp(cleaned_blob,
                      "LicensedbySonyComputerEntertainmentAmerica") == 0) {
      region = Region::north_america;
    }
    else if(strcmp(cleaned_blob,
		   "LicensedbySonyComputerEntertainmentEurope") == 0) {
      region = Region::europe;
    }
    else {
      LOG_ERROR("Couldn't identify disc region string");
      return std::nullopt;
    }

    return *this;
  }
};

inline std::optional<Disc> from_path(const char* path) {
  FILE *file = fopen(path, "r");

  Disc disc = {file, Region::japan};

  return disc.extract_region();
}
