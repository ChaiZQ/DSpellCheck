// This file is part of DSpellCheck Plug-in for Notepad++
// Copyright (C)2018 Sergey Semushin <Predelnik@gmail.com>
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "CommonFunctions.h"
#include "iconv.h"
#include "MainDef.h"
#include "Plugin.h"
#include <cassert>
#include "utils/utf8.h"
#include "MappedWString.h"
#include "resource.h"
#include <numeric>
#include "utils/string_utils.h"

static std::vector<char> convert(const char* source_enc, const char* target_enc, const void* source_data_ptr,
                                 size_t source_len, size_t max_dest_len) {
    std::vector<char> buf(max_dest_len);
    auto out_buf = reinterpret_cast<char *>(buf.data());
    iconv_t converter = iconv_open(target_enc, source_enc);
    auto char_ptr = static_cast<const char *>(source_data_ptr);
    size_t res =
        iconv(converter, static_cast<const char **>(&char_ptr), &source_len, &out_buf, &max_dest_len);
    iconv_close(converter);

    if (res == static_cast<size_t>(-1))
        return {};
    return buf;
}

MappedWstring utf8_to_mapped_wstring(std::string_view str) {
    if (str.empty())
        return {};
    ptrdiff_t len = str.length();
    std::vector<wchar_t> buf;
    std::vector<long> mapping;
    buf.reserve(len);
    mapping.reserve(len);
    auto it = str.data();
    // sadly this garbage skipping is required due to bad find prev mistake algorithm
    while (utf8_is_cont (*it))
      ++it;
    size_t char_cnt = 1;
    while (it - str.data() < len) {
        auto next = utf8_inc(it);
        buf.resize(char_cnt);
        MultiByteToWideChar(CP_UTF8, 0, it, static_cast<int>(next - it), buf.data() + char_cnt - 1, 1);
        mapping.push_back(static_cast<long> (it - str.data()));
        ++char_cnt;
        it = next;
    }
    mapping.push_back(static_cast<long> (it - str.data()));
    return {std::wstring{buf.begin(), buf.end()}, std::move(mapping)};
}

MappedWstring to_mapped_wstring(std::string_view str) {
    if (str.empty())
      return {};
    std::vector<long> mapping (str.length () + 1);
    std::iota (mapping.begin (), mapping.end (), 0);
    return {to_wstring(str), std::move (mapping)};
}

std::wstring to_wstring(std::string_view source) {
    auto bytes = convert("CHAR", "UCS-2LE//IGNORE", source.data(), source.length(),
                         sizeof(wchar_t) * (source.length() + 1));
    if (bytes.empty()) return {};
    return reinterpret_cast<wchar_t *>(bytes.data());
}

std::string to_string(std::wstring_view source) {
    auto bytes = convert("UCS-2LE", "CHAR//IGNORE", source.data(), (source.length()) * sizeof(wchar_t),
                         sizeof(wchar_t) * (source.length() + 1));
    if (bytes.empty()) return {};
    return bytes.data();
}

constexpr size_t max_utf8_char_length = 6;

std::string to_utf8_string(std::string_view source) {
    auto bytes = convert("CHAR", "UTF-8//IGNORE", source.data(), source.length(),
                         max_utf8_char_length * (source.length() + 1));
    if (bytes.empty()) return {};
    return bytes.data();
}

std::string to_utf8_string(std::wstring_view source) {
    auto bytes = convert("UCS-2LE", "UTF-8//IGNORE", source.data(), source.length() * sizeof (wchar_t),
                         max_utf8_char_length * (source.length() + 1));
    if (bytes.empty()) return {};
    return bytes.data();
}

std::wstring utf8_to_wstring(const char* source) {
    auto bytes = convert("UTF-8", "UCS-2LE//IGNORE", source, strlen(source),
                         (utf8_length(source) + 1) * sizeof (wchar_t));
    if (bytes.empty()) return {};
    return reinterpret_cast<const wchar_t *>(bytes.data());
}

std::string utf8_to_string(const char* source) {
    auto bytes = convert("UTF-8", "CHAR//IGNORE", source, strlen(source),
                         utf8_length(source) + 1);
    if (bytes.empty()) return {};
    return bytes.data();
}

// This function is more or less transferred from gcc source
bool match_special_char(wchar_t* dest, const wchar_t*& source) {
    wchar_t c;

    bool m = true;
    auto assign = [dest](wchar_t c) { if (dest != nullptr) *dest = c; };

    switch (c = *(source++)) {
    case L'a':
        assign(L'\a');
        break;
    case L'b':
        assign(L'\b');
        break;
    case L't':
        assign(L'\t');
        break;
    case L'f':
        assign(L'\f');
        break;
    case L'n':
        assign(L'\n');
        break;
    case L'r':
        assign(L'\r');
        break;
    case L'v':
        assign(L'\v');
        break;
    case L'\\':
        assign(L'\\');
        break;
    case L'0':
        assign(L'\0');
        break;

    case L'x':
    case L'u':
    case L'U':
        {
            /* Hexadecimal form of wide characters.  */
            int len = (c == L'x' ? 2 : (c == L'u' ? 4 : 8));
            wchar_t n = 0;
#ifndef UNICODE
    len = 2;
#endif
            for (int i = 0; i < len; i++) {
                wchar_t buf[2] = {L'\0', L'\0'};

                c = *(source++);
                if (c > UCHAR_MAX ||
                    !((L'0' <= c && c <= L'9') || (L'a' <= c && c <= L'f') ||
                        (L'A' <= c && c <= L'F')))
                    return false;

                buf[0] = static_cast<wchar_t>(c);
                n = n << 4;
                n += static_cast<wchar_t>(wcstol(buf, nullptr, 16));
            }
            assign(n);
            break;
        }

    default:
        /* Unknown backslash codes are simply not expanded.  */
        m = false;
        break;
    }
    return m;
}

static size_t do_parse_string(wchar_t* dest, const wchar_t* source) {
    bool dry_run = dest == nullptr;
    wchar_t* res_string = dest;
    while (*source != L'\0') {
        const wchar_t* last_pos = source;
        if (*source == '\\') {
            ++source;
            if (!match_special_char(!dry_run ? dest : nullptr, source)) {
                source = last_pos;
                if (!dry_run)
                    *dest = *source;
                ++source;
                ++dest;
            }
            else {
                ++dest;
            }
        }
        else {
            if (!dry_run)
                *dest = *source;
            ++source;
            ++dest;
        }
    }
    if (!dry_run)
        *dest = L'\0';
    return dest - res_string + 1;
}

std::wstring parse_string(const wchar_t* source) {
    auto size = do_parse_string(nullptr, source);
    std::vector<wchar_t> buf(size);
    do_parse_string(buf.data(), source);
    assert (size == buf.size ());
    return buf.data();
}

// These functions are mostly taken from http://research.microsoft.com

static const std::unordered_map<std::wstring_view, std::wstring_view> alias_map = {
    {L"AF_ZA", L"Afrikaans"},
    {L"AK_GH", L"Akan"},
    {L"AN_ES", L"Aragonese"},
    {L"AR", L"Arabic"},
    {L"BE_BY", L"Belarusian"},
    {L"BN_BD", L"Bengali"},
    {L"BO", L"Classical Tibetan"},
    {L"BG_BG", L"Bulgarian"},
    {L"BR_FR", L"Breton"},
    {L"BS_BA", L"Bosnian"},
    {L"CA", L"Catalan"},
    {L"CA_VALENCIA", L"Catalan (Valencia)"},
    {L"CA_ANY", L"Catalan (Any)"},
    {L"CA_ES", L"Catalan (Spain)"},
    {L"COP_EG", L"Coptic (Bohairic dialect)"},
    {L"CS_CZ", L"Czech"},
    {L"CY_GB", L"Welsh"},
    {L"DA_DK", L"Danish"},
    {L"DE_AT", L"German (Austria)"},
    {L"DE_AT_FRAMI", L"German (Austria) [frami]"},
    {L"DE_CH", L"German (Switzerland)"},
    {L"DE_CH_FRAMI", L"German (Switzerland) [frami]"},
    {L"DE_DE", L"German (Germany)"},
    {L"DE_DE_COMB", L"German (Old and New Spelling)"},
    {L"DE_DE_FRAMI", L"German (Germany) [frami]"},
    {L"DE_DE_NEU", L"German (New Spelling)"},
    {L"EL_GR", L"Greek"},
    {L"EN_AU", L"English (Australia)"},
    {L"EN_CA", L"English (Canada)"},
    {L"EN_GB", L"English (Great Britain)"},
    {L"EN_GB_OED", L"English (Great Britain) [OED]"},
    {L"EN_NZ", L"English (New Zealand)"},
    {L"EN_US", L"English (United States)"},
    {L"EN_ZA", L"English (South Africa)"},
    {L"EO_EO", L"Esperanto"},
    {L"ES_ANY", L"Spanish"},
    {L"ES_AR", L"Spanish (Argentina)"},
    {L"ES_BO", L"Spanish (Bolivia)"},
    {L"ES_CL", L"Spanish (Chile)"},
    {L"ES_CO", L"Spanish (Colombia)"},
    {L"ES_CR", L"Spanish (Costa Rica)"},
    {L"ES_CU", L"Spanish (Cuba)"},
    {L"ES_DO", L"Spanish (Dominican Republic)"},
    {L"ES_EC", L"Spanish (Ecuador)"},
    {L"ES_ES", L"Spanish (Spain)"},
    {L"ES_GT", L"Spanish (Guatemala)"},
    {L"ES_HN", L"Spanish (Honduras)"},
    {L"ES_MX", L"Spanish (Mexico)"},
    {L"ES_NEW", L"Spanish (New)"},
    {L"ES_NI", L"Spanish (Nicaragua)"},
    {L"ES_PA", L"Spanish (Panama)"},
    {L"ES_PE", L"Spanish (Peru)"},
    {L"ES_PR", L"Spanish (Puerto Rico)"},
    {L"ES_PY", L"Spanish (Paraguay)"},
    {L"ES_SV", L"Spanish (El Salvador)"},
    {L"ES_UY", L"Spanish (Uruguay)"},
    {L"ES_VE", L"Spanish (Bolivarian Republic of Venezuela)"},
    {L"ET_EE", L"Estonian"},
    {L"FO_FO", L"Faroese"},
    {L"FR", L"French"},
    {L"FR_FR", L"French"},
    {L"FR_FR_1990", L"French (1990)"},
    {L"FR_FR_1990_1_3_2", L"French (1990)"},
    {L"FR_FR_CLASSIQUE", L"French (Classique)"},
    {L"FR_FR_CLASSIQUE_1_3_2", L"French (Classique)"},
    {L"FR_FR_1_3_2", L"French"},
    {L"FY_NL", L"Frisian"},
    {L"GA_IE", L"Irish"},
    {L"GD_GB", L"Scottish Gaelic"},
    {L"GL_ES", L"Galician"},
    {L"GU_IN", L"Gujarati"},
    {L"GUG", L"Guarani"},
    {L"HE_IL", L"Hebrew"},
    {L"HI_IN", L"Hindi"},
    {L"HIl_PH", L"Hiligaynon"},
    {L"HR_HR", L"Croatian"},
    {L"HU_HU", L"Hungarian"},
    {L"IA", L"Interlingua"},
    {L"ID_ID", L"Indonesian"},
    {L"IS", L"Icelandic"},
    {L"IS_IS", L"Icelandic"},
    {L"IT_IT", L"Italian"},
    {L"KMR_LATN", L"Kurdish (Latin)"},
    {L"KU_TR", L"Kurdish"},
    {L"LA", L"Latin"},
    {L"LO_LA", L"Lao"},
    {L"LT", L"Lithuanian"},
    {L"LV_LV", L"Latvian"},
    {L"MG_MG", L"Malagasy"},
    {L"MI_NZ", L"Maori"},
    {L"MK_MK", L"Macedonian (FYROM)"},
    {L"MOS_BF", L"Mossi"},
    {L"MR_IN", L"Marathi"},
    {L"MS_MY", L"Malay"},
    {L"NB_NO", L"Norwegian (Bokmal)"},
    {L"NE_NP", L"Nepali"},
    {L"NL_NL", L"Dutch"},
    {L"NN_NO", L"Norwegian (Nynorsk)"},
    {L"NR_ZA", L"Ndebele"},
    {L"NS_ZA", L"Northern Sotho"},
    {L"NY_MW", L"Chichewa"},
    {L"OC_FR", L"Occitan"},
    {L"PL_PL", L"Polish"},
    {L"PT_BR", L"Portuguese (Brazil)"},
    {L"PT_PT", L"Portuguese (Portugal)"},
    {L"RO_RO", L"Romanian"},
    {L"RU_RU", L"Russian"},
    {L"RU_RU_IE", L"Russian (without io)"},
    {L"RU_RU_YE", L"Russian (without io)"},
    {L"RU_RU_YO", L"Russian (with io)"},
    {L"RW_RW", L"Kinyarwanda"},
    {L"SI_LK", L"Sinhala"},
    {L"SI_SI", L"Slovenian"},
    {L"SK_SK", L"Slovak"},
    {L"SL_SI", L"Slovenian"},
    {L"SQ_AL", L"Alban"},
    {L"SR"   , L"Serbian (Cyrillic)"},
    {L"SR_LATN", L"Serbian (Latin)"},
    {L"SS_ZA", L"Swazi"},
    {L"ST_ZA", L"Northern Sotho"},
    {L"SV_SE", L"Swedish"},
    {L"SV_FI", L"Swedish (Finland)"},
    {L"SW_KE", L"Kiswahili"},
	{L"SW_TZ", L"Swahili"},
	{L"TE_IN", L"Telugu"},
    {L"TET_ID", L"Tetum"},
    {L"TH_TH", L"Thai"},
    {L"TL_PH", L"Tagalog"},
    {L"TN_ZA", L"Setswana"},
    {L"TS_ZA", L"Tsonga"},
    {L"UK_UA", L"Ukrainian"},
    {L"UR_PK", L"Urdu"},
    {L"VE_ZA", L"Venda"},
    {L"VI_VN", L"Vietnamese"},
    {L"XH_ZA", L"isiXhosa"},
    {L"ZU_ZA", L"isiZulu"}
};
// Language Aliases
std::pair<std::wstring_view, bool> apply_alias(std::wstring_view str) {
  std::wstring str_normalized {str};
  to_upper_inplace(str_normalized);
  std::replace(str_normalized.begin(), str_normalized.end(), L'-', L'_');
    auto it = alias_map.find(str_normalized);
    if (it != alias_map.end())
        return {it->second, true};

    return {str, false};
}

static bool try_to_create_dir(const wchar_t* path, bool silent, HWND npp_window) {
    if (!CreateDirectory(path, nullptr)) {
        if (!silent) {
            if (npp_window == nullptr)
                return false;

            wchar_t message[DEFAULT_BUF_SIZE];
            swprintf(message, rc_str(IDS_CANT_CREATE_DIR_PS).c_str (), path);
            MessageBox(npp_window, message, rc_str(IDS_ERROR_IN_DIR_CREATE).c_str (),
                       MB_OK | MB_ICONERROR);
        }
        return false;
    }
    return true;
}

bool check_for_directory_existence(std::wstring path, bool silent, HWND npp_window) {
    for (auto& c : path) {
        if (c == L'\\') {
            c = L'\0';
            if (!PathFileExists(path.c_str())) {
                if (!try_to_create_dir(path.c_str(), silent, npp_window))
                    return false;
            }
            c = L'\\';
        }
    }
    if (!PathFileExists(path.c_str())) {
        if (!try_to_create_dir(path.c_str(), silent, npp_window))
            return false;
    }
    return true;
}

void write_unicode_bom (FILE *fp)
{
    WORD bom = 0xFEFF;
    fwrite(&bom, sizeof(bom), 1, fp);
}

std::wstring read_ini_value(const wchar_t* app_name, const wchar_t* key_name, const wchar_t* default_value,
                            const wchar_t* file_name) {
    constexpr int initial_buffer_size = 64;
    std::vector<wchar_t> buffer(initial_buffer_size);
    while (true) {
        auto size_written = GetPrivateProfileString(app_name, key_name, default_value, buffer.data(),
                                                    static_cast<DWORD>(buffer.size()), file_name);
        if (size_written < buffer.size() - 1)
            return buffer.data();
        buffer.resize(buffer.size() * 2);
    }
}
