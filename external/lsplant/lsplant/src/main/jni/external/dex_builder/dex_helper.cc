#include "dex_helper.h"

#include <algorithm>

#include "slicer/dex_format.h"
#include "slicer/dex_leb128.h"
#include "slicer/reader.h"
#include "slicer/dex_utf8.h"

namespace {
constexpr auto utf8_less = [](const std::string_view a, const std::string_view b) { return dex::Utf8Cmp(a.data(), b.data()) < 0; };
}  // namespace

DexHelper::DexHelper(const std::vector<std::tuple<const void *, size_t, const void *, size_t>> &dexs) {
    for (const auto &[image, size, data, data_size] : dexs) {
        readers_.emplace_back(static_cast<const dex::u1 *>(image), size, static_cast<const dex::u1 *>(data), data_size);
    }
    auto dex_count = readers_.size();

    // init
    rev_method_indices_.resize(dex_count);
    rev_class_indices_.resize(dex_count);
    rev_field_indices_.resize(dex_count);
    strings_.resize(dex_count);
    method_codes_.resize(dex_count);
    string_cache_.resize(dex_count);
    type_cache_.resize(dex_count);
    field_cache_.resize(dex_count);
    method_cache_.resize(dex_count);
    class_cache_.resize(dex_count);
    invoking_cache_.resize(dex_count);
    invoked_cache_.resize(dex_count);
    getting_cache_.resize(dex_count);
    setting_cache_.resize(dex_count);
    declaring_cache_.resize(dex_count);
    searched_methods_.resize(dex_count);

    for (auto dex_idx = 0zu; dex_idx < dex_count; ++dex_idx) {
        auto &dex = readers_[dex_idx];
        rev_method_indices_[dex_idx].resize(dex.MethodIds().size(), size_t(-1));
        rev_class_indices_[dex_idx].resize(dex.TypeIds().size(), size_t(-1));
        rev_field_indices_[dex_idx].resize(dex.FieldIds().size(), size_t(-1));

        strings_[dex_idx].reserve(dex.StringIds().size());
        method_codes_[dex_idx].resize(dex.MethodIds().size(), nullptr);

        type_cache_[dex_idx].resize(dex.StringIds().size(), dex::kNoIndex);
        field_cache_[dex_idx].resize(dex.TypeIds().size());
        method_cache_[dex_idx].resize(dex.TypeIds().size());
        class_cache_[dex_idx].resize(dex.TypeIds().size(), dex::kNoIndex);

        string_cache_[dex_idx].resize(dex.StringIds().size());
        invoking_cache_[dex_idx].resize(dex.MethodIds().size());
        invoked_cache_[dex_idx].resize(dex.MethodIds().size());
        getting_cache_[dex_idx].resize(dex.FieldIds().size());
        setting_cache_[dex_idx].resize(dex.FieldIds().size());
        declaring_cache_[dex_idx].resize(dex.TypeIds().size());

        searched_methods_[dex_idx].resize(dex.MethodIds().size());
    }

    for (auto dex_idx = 0zu; dex_idx < dex_count; ++dex_idx) {
        auto &dex = readers_[dex_idx];
        auto &strs = strings_[dex_idx];
        for (const auto &str : dex.StringIds()) {
            const auto *ptr = dex.dataPtr<dex::u1>(str.string_data_off);
            dex::ReadULeb128(&ptr);
            strs.emplace_back(reinterpret_cast<const char *>(ptr));
        }
    }

    for (auto dex_idx = 0zu; dex_idx < dex_count; ++dex_idx) {
        auto &dex = readers_[dex_idx];
        for (auto class_idx = 0zu; class_idx < dex.ClassDefs().size(); ++class_idx) {
            const auto &class_def = dex.ClassDefs()[class_idx];
            class_cache_[dex_idx][class_def.class_idx] = class_idx;
            if (class_def.class_data_off == 0) continue;
            const auto *class_data = dex.dataPtr<dex::u1>(class_def.class_data_off);
            dex::u4 static_fields_count = dex::ReadULeb128(&class_data);
            dex::u4 instance_fields_count = dex::ReadULeb128(&class_data);
            dex::u4 direct_methods_count = dex::ReadULeb128(&class_data);
            dex::u4 virtual_methods_count = dex::ReadULeb128(&class_data);

            auto &codes = method_codes_[dex_idx];
            codes.resize(dex.MethodIds().size(), nullptr);

            for (dex::u4 i = 0; i < static_fields_count; ++i) {
                dex::ReadULeb128(&class_data);
                dex::ReadULeb128(&class_data);
            }

            for (dex::u4 i = 0; i < instance_fields_count; ++i) {
                dex::ReadULeb128(&class_data);
                dex::ReadULeb128(&class_data);
            }

            for (dex::u4 i = 0, method_idx = 0; i < direct_methods_count; ++i) {
                method_idx += dex::ReadULeb128(&class_data);
                dex::ReadULeb128(&class_data);
                auto offset = dex::ReadULeb128(&class_data);
                if (offset != 0) {
                    codes[method_idx] = dex.dataPtr<const dex::CodeItem>(offset);
                }
            }

            for (dex::u4 i = 0, method_idx = 0; i < virtual_methods_count; ++i) {
                method_idx += dex::ReadULeb128(&class_data);
                dex::ReadULeb128(&class_data);
                auto offset = dex::ReadULeb128(&class_data);
                if (offset != 0) {
                    codes[method_idx] = dex.dataPtr<dex::CodeItem>(offset);
                }
            }
        }
    }
    for (auto dex_idx = 0zu; dex_idx < dex_count; ++dex_idx) {
        auto &dex = readers_[dex_idx];
        auto &type = type_cache_[dex_idx];
        auto &field = field_cache_[dex_idx];
        auto &declare = declaring_cache_[dex_idx];
        auto &method = method_cache_[dex_idx];
        for (auto type_idx = 0zu; type_idx < dex.TypeIds().size(); ++type_idx) {
            type[dex.TypeIds()[type_idx].descriptor_idx] = type_idx;
        }
        for (auto field_idx = 0zu; field_idx < dex.FieldIds().size(); ++field_idx) {
            auto f = dex.FieldIds()[field_idx];
            field[f.class_idx][f.name_idx] = field_idx;
            declare[f.type_idx].emplace_back(field_idx);
        }
        for (auto method_idx = 0zu; method_idx < dex.MethodIds().size(); ++method_idx) {
            auto m = dex.MethodIds()[method_idx];
            method[m.class_idx][m.name_idx].emplace_back(method_idx);
        }
    }
}

std::tuple<uint32_t, uint32_t> DexHelper::FindPrefixStringId(size_t dex_idx,
                                                             std::string_view to_find) const {
    const auto &strs = strings_[dex_idx];
    if (auto str_lower_bound = std::lower_bound(strs.cbegin(), strs.cend(), to_find, utf8_less),
        str_upper_bound =
            std::upper_bound(strs.cbegin(), strs.cend(), std::string(to_find) + '\xff', utf8_less);
        str_upper_bound != strs.cend() && str_lower_bound != strs.cend() &&
        str_lower_bound <= str_upper_bound) {
        return {str_lower_bound - strs.cbegin(), str_upper_bound - strs.cbegin()};
    }
    return {dex::kNoIndex, dex::kNoIndex};
}

uint32_t DexHelper::FindPrefixStringIdExact(size_t dex_idx, std::string_view to_find) const {
    const auto &strs = strings_[dex_idx];
    auto first = std::lower_bound(strs.cbegin(), strs.cend(), to_find, utf8_less);
    if (first != strs.cend() && *first == to_find) {
        return first - strs.cbegin();
    }
    return dex::kNoIndex;
}

void DexHelper::CreateFullCache() const {
    for (auto dex_idx = 0zu; dex_idx < readers_.size(); ++dex_idx) {
        const auto &codes = method_codes_[dex_idx];
        for (auto method_id = 0zu; method_id < codes.size(); ++method_id) {
            ScanMethod(dex_idx, method_id);
        }
    }
}

bool DexHelper::ScanMethod(size_t dex_idx, uint32_t method_id, size_t str_lower,
                           size_t str_upper) const {
    static constexpr dex::u1 kOpcodeMask = 0xff;
    static constexpr dex::u1 kOpcodeNoOp = 0x00;
    static constexpr dex::u1 kOpcodeConstString = 0x1a;
    static constexpr dex::u1 kOpcodeConstStringJumbo = 0x1b;
    static constexpr dex::u1 kOpcodeIGetStart = 0x52;
    static constexpr dex::u1 kOpcodeIGetEnd = 0x58;
    static constexpr dex::u1 kOpcodeSGetStart = 0x60;
    static constexpr dex::u1 kOpcodeSGetEnd = 0x66;
    static constexpr dex::u1 kOpcodeIPutStart = 0x59;
    static constexpr dex::u1 kOpcodeIPutEnd = 0x5f;
    static constexpr dex::u1 kOpcodeSPutStart = 0x67;
    static constexpr dex::u1 kOpcodeSPutEnd = 0x6d;
    static constexpr dex::u1 kOpcodeInvokeStart = 0x6e;
    static constexpr dex::u1 kOpcodeInvokeEnd = 0x72;
    static constexpr dex::u1 kOpcodeInvokeRangeStart = 0x74;
    static constexpr dex::u1 kOpcodeInvokeRangeEnd = 0x78;
    static constexpr dex::u2 kInstPackedSwitchPlayLoad = 0x0100;
    static constexpr dex::u2 kInstSparseSwitchPlayLoad = 0x0200;
    static constexpr dex::u2 kInstFillArrayDataPlayLoad = 0x0300;
    auto &dex = readers_[dex_idx];
    auto &str_cache = string_cache_[dex_idx];
    auto &inv_cache = invoking_cache_[dex_idx];
    auto &inved_cache = invoked_cache_[dex_idx];
    auto &get_cache = getting_cache_[dex_idx];
    auto &set_cache = setting_cache_[dex_idx];
    auto &scanned = searched_methods_[dex_idx];

    bool match_str = false;
    if (scanned[method_id]) {
        return match_str;
    }
    scanned[method_id] = true;
    const auto &code = method_codes_[dex_idx][method_id];
    if (!code) {
        return match_str;
    }
    const dex::u2 *inst;
    const dex::u2 *end;
    if (dex.IsCompact()) {
        auto insns_count_and_flags =  reinterpret_cast<const dex::CompactCode*>(code)->insns_count_and_flags;
        inst = reinterpret_cast<const dex::CompactCode*>(code)->insns;
        dex::u4 insns_count = (insns_count_and_flags >> dex::CompactCode::kInsnsSizeShift);
        if (insns_count_and_flags & dex::CompactCode::kFlagPreHeaderInsnsSize) {
            const auto *preheader = reinterpret_cast<const uint16_t*>(code);
            --preheader;
            insns_count += static_cast<uint32_t>(*preheader);
            --preheader;
            insns_count += static_cast<uint32_t>(*preheader) << 16;
        }
        end = inst + insns_count;
    } else {
        inst = reinterpret_cast<const dex::Code*>(code)->insns;
        end = inst + reinterpret_cast<const dex::Code*>(code)->insns_size;
    }
    while (inst < end) {
        dex::u1 opcode = *inst & kOpcodeMask;
        if (opcode == kOpcodeConstString) {
            auto str_idx = inst[1];
            if (str_lower <= str_idx && str_upper > str_idx) {
                match_str = true;
            }
            str_cache[str_idx].emplace_back(method_id);
        }
        if (opcode == kOpcodeConstStringJumbo) {
            auto str_idx = *reinterpret_cast<const dex::u4 *>(&inst[1]);
            if (str_lower <= str_idx && str_upper > str_idx) {
                match_str = true;
            }
            str_cache[str_idx].emplace_back(method_id);
        }
        if ((opcode >= kOpcodeIGetStart && opcode <= kOpcodeIGetEnd) ||
            (opcode >= kOpcodeSGetStart && opcode <= kOpcodeSGetEnd)) {
            auto field_idx = inst[1];
            get_cache[field_idx].emplace_back(method_id);
        }
        if ((opcode >= kOpcodeIPutStart && opcode <= kOpcodeIPutEnd) ||
            (opcode >= kOpcodeSPutStart && opcode <= kOpcodeSPutEnd)) {
            auto field_idx = inst[1];
            set_cache[field_idx].emplace_back(method_id);
        }
        if ((opcode >= kOpcodeInvokeStart && opcode <= kOpcodeInvokeEnd) ||
            (opcode >= kOpcodeInvokeRangeStart && opcode <= kOpcodeInvokeRangeEnd)) {
            auto callee = inst[1];
            inv_cache[method_id].emplace_back(callee);
            inved_cache[callee].emplace_back(method_id);
        }
        if (opcode == kOpcodeNoOp) {
            if (*inst == kInstPackedSwitchPlayLoad) {
                inst += inst[1] * 2 + 3;
            } else if (*inst == kInstSparseSwitchPlayLoad) {
                inst += inst[1] * 4 + 1;
            } else if (*inst == kInstFillArrayDataPlayLoad) {
                inst += (*reinterpret_cast<const dex::u4 *>(&inst[2]) * inst[1] + 1) / 2 + 3;
            }
        }
        inst += dex::opcode_len[opcode];
    }
    return match_str;
}

std::tuple<std::vector<std::vector<uint32_t>>, std::vector<std::vector<uint32_t>>>
DexHelper::ConvertParameters(const std::vector<size_t> &parameter_types,
                             const std::vector<size_t> &contains_parameter_types) const {
    std::vector<std::vector<uint32_t>> parameter_types_ids(readers_.size());
    std::vector<std::vector<uint32_t>> contains_parameter_types_ids(readers_.size());
    if (!parameter_types.empty()) {
        for (auto dex_idx = 0zu; dex_idx < readers_.size(); ++dex_idx) {
            parameter_types_ids[dex_idx].reserve(parameter_types.size());
        }
        for (const auto &param : parameter_types) {
            if (param != size_t(-1) && param >= class_indices_.size()) {
                return {parameter_types_ids, contains_parameter_types_ids};
            }
            if (param == size_t(-1)) {
                for (auto dex_idx = 0zu; dex_idx < readers_.size(); ++dex_idx) {
                    parameter_types_ids[dex_idx].emplace_back(-2);
                }
                break;
            }
            auto &ids = class_indices_[param];
            for (auto dex_idx = 0zu; dex_idx < readers_.size(); ++dex_idx) {
                parameter_types_ids[dex_idx].emplace_back(ids[dex_idx]);
            }
        }
    }

    if (!contains_parameter_types.empty()) {
        for (auto dex_idx = 0zu; dex_idx < readers_.size(); ++dex_idx) {
            contains_parameter_types_ids[dex_idx].reserve(contains_parameter_types.size());
        }
        for (const auto &param : contains_parameter_types) {
            if (param != size_t(-1) && param >= class_indices_.size()) {
                return {parameter_types_ids, contains_parameter_types_ids};
            }
            auto &ids = class_indices_[param];
            for (auto dex_idx = 0zu; dex_idx < readers_.size(); ++dex_idx) {
                contains_parameter_types_ids[dex_idx].emplace_back(ids[dex_idx]);
            }
        }
    }
    return {parameter_types_ids, contains_parameter_types_ids};
}

std::vector<size_t> DexHelper::FindMethodUsingString(
    std::string_view str, bool match_prefix, size_t return_type, short parameter_count,
    std::string_view parameter_shorty, size_t declaring_class,
    const std::vector<size_t> &parameter_types, const std::vector<size_t> &contains_parameter_types,
    const std::vector<size_t> &dex_priority, bool find_first) const {
    std::vector<size_t> out;

    if (return_type != size_t(-1) && return_type >= class_indices_.size()) return out;
    if (declaring_class != size_t(-1) && declaring_class >= class_indices_.size()) return out;
    const auto [parameter_types_ids, contains_parameter_types_ids] =
        ConvertParameters(parameter_types, contains_parameter_types);

    for (auto dex_idx : GetPriority(dex_priority)) {
        uint32_t lower;
        uint32_t upper;
        if (match_prefix) {
            std::tie(lower, upper) = FindPrefixStringId(dex_idx, str);
            if (lower == dex::kNoIndex) continue;
        } else {
            lower = upper = FindPrefixStringIdExact(dex_idx, str);
            if (lower == dex::kNoIndex) continue;
            ++upper;
        }
        const auto &codes = method_codes_[dex_idx];
        const auto &strs = string_cache_[dex_idx];
        const auto return_type_id = return_type == size_t(-1) ? uint32_t(-2) : class_indices_[return_type][dex_idx];
        const auto declaring_class_id = declaring_class == size_t(-1) ? uint32_t(-2): class_indices_[declaring_class][dex_idx];

        if (find_first) {
            for (auto s = lower; s < upper; ++s) {
                for (const auto &m : strs[s]) {
                    if (IsMethodMatch(dex_idx, m,
                                      return_type_id,
                                      parameter_count, parameter_shorty,
                                      declaring_class_id,
                                      parameter_types_ids[dex_idx],
                                      contains_parameter_types_ids[dex_idx])) {
                        out.emplace_back(CreateMethodIndex(dex_idx, m));
                        return out;
                    }
                }
            }
        }

        for (auto method_id = 0zu; method_id < codes.size(); ++method_id) {
            auto &scanned = searched_methods_[dex_idx];
            if (scanned[method_id]) continue;
            if (IsMethodMatch(
                    dex_idx, method_id,
                    return_type_id,
                    parameter_count, parameter_shorty,
                    declaring_class_id,
                    parameter_types_ids[dex_idx], contains_parameter_types_ids[dex_idx])) {
                bool match = ScanMethod(dex_idx, method_id, lower, upper);
                if (match && find_first) break;
            }
        }

        for (auto s = lower; s < upper; ++s) {
            for (const auto &m : strs[s]) {
                if (IsMethodMatch(dex_idx, m,
                        return_type_id,
                                 parameter_count, parameter_shorty,
                                 declaring_class_id,
                                 parameter_types_ids[dex_idx],
                                 contains_parameter_types_ids[dex_idx])) {
                    out.emplace_back(CreateMethodIndex(dex_idx, m));
                    if (find_first) return out;
                }
            }
        }
    }
    return out;
}

std::vector<size_t> DexHelper::FindMethodInvoking(
    size_t method_idx, size_t return_type, short parameter_count, std::string_view parameter_shorty,
    size_t declaring_class, const std::vector<size_t> &parameter_types,
    const std::vector<size_t> &contains_parameter_types, const std::vector<size_t> &dex_priority,
    bool find_first) const {
    std::vector<size_t> out;

    if (method_idx >= method_indices_.size()) return out;
    if (return_type != size_t(-1) && return_type >= class_indices_.size()) return out;
    if (declaring_class != size_t(-1) && declaring_class >= class_indices_.size()) return out;
    const auto [parameter_types_ids, contains_parameter_types_ids] =
        ConvertParameters(parameter_types, contains_parameter_types);

    const auto method_ids = method_indices_[method_idx];

    for (auto dex_idx : GetPriority(dex_priority)) {
        auto caller_id = method_ids[dex_idx];
        if (caller_id == dex::kNoIndex) continue;
        const auto return_type_id = return_type == size_t(-1) ? uint32_t(-2) : class_indices_[return_type][dex_idx];
        const auto declaring_class_id = declaring_class == size_t(-1) ? uint32_t(-2): class_indices_[declaring_class][dex_idx];
        ScanMethod(dex_idx, caller_id);
        for (auto callee : invoking_cache_[dex_idx][caller_id]) {
            if (IsMethodMatch(
                    dex_idx, callee,
                    return_type_id,
                    parameter_count, parameter_shorty,
                    declaring_class_id,
                    parameter_types_ids[dex_idx], contains_parameter_types_ids[dex_idx])) {
                out.emplace_back(CreateMethodIndex(dex_idx, callee));
                if (find_first) return out;
            }
        }
    }
    return out;
}

std::vector<size_t> DexHelper::FindMethodInvoked(
    size_t method_idx, size_t return_type, short parameter_count, std::string_view parameter_shorty,
    size_t declaring_class, const std::vector<size_t> &parameter_types,
    const std::vector<size_t> &contains_parameter_types, const std::vector<size_t> &dex_priority,
    bool find_first) const {
    std::vector<size_t> out;

    if (method_idx >= method_indices_.size()) return out;
    if (return_type != size_t(-1) && return_type >= class_indices_.size()) return out;
    if (declaring_class != size_t(-1) && declaring_class >= class_indices_.size()) return out;
    const auto [parameter_types_ids, contains_parameter_types_ids] =
        ConvertParameters(parameter_types, contains_parameter_types);

    const auto method_ids = method_indices_[method_idx];

    for (auto dex_idx : GetPriority(dex_priority)) {
        auto callee_id = method_ids[dex_idx];
        if (callee_id == dex::kNoIndex) continue;
        const auto &codes = method_codes_[dex_idx];
        const auto &cache = invoked_cache_[dex_idx][callee_id];
        const auto return_type_id = return_type == size_t(-1) ? uint32_t(-2) : class_indices_[return_type][dex_idx];
        const auto declaring_class_id = declaring_class == size_t(-1) ? uint32_t(-2): class_indices_[declaring_class][dex_idx];
        if (find_first && !cache.empty()) {
            for(const auto &caller : cache) {
                if (IsMethodMatch(dex_idx, caller,
                                  return_type_id,
                                  parameter_count, parameter_shorty,
                                  declaring_class_id,
                                  parameter_types_ids[dex_idx],
                                  contains_parameter_types_ids[dex_idx])) {
                    out.emplace_back(CreateMethodIndex(dex_idx, caller));
                    return out;
                }
            }
        }
        for (auto method_id = 0zu; method_id < codes.size(); ++method_id) {
            auto &scanned = searched_methods_[dex_idx];
            if (scanned[method_id]) continue;
            if (IsMethodMatch(
                    dex_idx, method_id,
                    return_type_id,
                    parameter_count, parameter_shorty,
                    declaring_class_id,
                    parameter_types_ids[dex_idx], contains_parameter_types_ids[dex_idx])) {
                ScanMethod(dex_idx, method_id);
                if (find_first && !cache.empty()) break;
            }
        }
        for (const auto &caller : cache) {
            if (IsMethodMatch(dex_idx, caller,
                              return_type_id,
                              parameter_count, parameter_shorty,
                              declaring_class_id,
                              parameter_types_ids[dex_idx],
                              contains_parameter_types_ids[dex_idx])) {
                out.emplace_back(CreateMethodIndex(dex_idx, caller));
                if (find_first) return out;
            }
        }
    }
    return out;
}

std::vector<size_t> DexHelper::FindMethodGettingField(
    size_t field_idx, size_t return_type, short parameter_count, std::string_view parameter_shorty,
    size_t declaring_class, const std::vector<size_t> &parameter_types,
    const std::vector<size_t> &contains_parameter_types, const std::vector<size_t> &dex_priority,
    bool find_first) const {
    std::vector<size_t> out;

    if (field_idx >= field_indices_.size()) return out;
    if (return_type != size_t(-1) && return_type >= class_indices_.size()) return out;
    if (declaring_class != size_t(-1) && declaring_class >= class_indices_.size()) return out;
    const auto [parameter_types_ids, contains_parameter_types_ids] =
        ConvertParameters(parameter_types, contains_parameter_types);
    auto field_ids = field_indices_[field_idx];
    for (auto dex_idx : GetPriority(dex_priority)) {
        auto field_id = field_ids[dex_idx];
        if (field_id == dex::kNoIndex) continue;
        const auto &codes = method_codes_[dex_idx];
        const auto &cache = getting_cache_[dex_idx][field_id];
        const auto return_type_id = return_type == size_t(-1) ? uint32_t(-2) : class_indices_[return_type][dex_idx];
        const auto declaring_class_id = declaring_class == size_t(-1) ? uint32_t(-2): class_indices_[declaring_class][dex_idx];
        if (find_first && !cache.empty()) {
            for (const auto &getter : cache) {
                if (IsMethodMatch(dex_idx, getter,
                                  return_type_id,
                                  parameter_count, parameter_shorty,
                                  declaring_class_id,
                                  parameter_types_ids[dex_idx],
                                  contains_parameter_types_ids[dex_idx])) {
                    out.emplace_back(CreateMethodIndex(dex_idx, getter));
                    return out;
                }
            }
        }
        for (auto method_id = 0zu; method_id < codes.size(); ++method_id) {
            auto &scanned = searched_methods_[dex_idx];
            if (scanned[method_id]) continue;
            if (IsMethodMatch(
                    dex_idx, method_id,
                    return_type_id,
                    parameter_count, parameter_shorty,
                    declaring_class_id,
                    parameter_types_ids[dex_idx], contains_parameter_types_ids[dex_idx])) {
                ScanMethod(dex_idx, method_id);
                if (find_first && !cache.empty()) break;
            }
        }
        for (const auto &getter : cache) {
            if (IsMethodMatch(dex_idx, getter,
                              return_type_id,
                              parameter_count, parameter_shorty,
                              declaring_class_id,
                              parameter_types_ids[dex_idx],
                              contains_parameter_types_ids[dex_idx])) {
                out.emplace_back(CreateMethodIndex(dex_idx, getter));
                if (find_first) return out;
            }
        }
    }
    return out;
}

std::vector<size_t> DexHelper::FindMethodSettingField(
    size_t field_idx, size_t return_type, short parameter_count, std::string_view parameter_shorty,
    size_t declaring_class, const std::vector<size_t> &parameter_types,
    const std::vector<size_t> &contains_parameter_types, const std::vector<size_t> &dex_priority,
    bool find_first) const {
    std::vector<size_t> out;

    if (field_idx >= field_indices_.size()) return out;
    if (return_type != size_t(-1) && return_type >= class_indices_.size()) return out;
    if (declaring_class != size_t(-1) && declaring_class >= class_indices_.size()) return out;
    const auto [parameter_types_ids, contains_parameter_types_ids] =
        ConvertParameters(parameter_types, contains_parameter_types);
    auto field_ids = field_indices_[field_idx];
    for (auto dex_idx : GetPriority(dex_priority)) {
        auto field_id = field_ids[dex_idx];
        if (field_id == dex::kNoIndex) continue;
        const auto &codes = method_codes_[dex_idx];
        const auto &cache = setting_cache_[dex_idx][field_id];
        const auto return_type_id = return_type == size_t(-1) ? uint32_t(-2) : class_indices_[return_type][dex_idx];
        const auto declaring_class_id = declaring_class == size_t(-1) ? uint32_t(-2): class_indices_[declaring_class][dex_idx];
        if (find_first && !cache.empty()) {
            for (const auto &setter : cache) {
                if (IsMethodMatch(dex_idx, setter,
                                  return_type_id,
                                  parameter_count, parameter_shorty,
                                  declaring_class_id,
                                  parameter_types_ids[dex_idx],
                                  contains_parameter_types_ids[dex_idx])) {
                    out.emplace_back(CreateMethodIndex(dex_idx, setter));
                    return out;
                }
            }
        }
        for (auto method_id = 0zu; method_id < codes.size(); ++method_id) {
            auto &scanned = searched_methods_[dex_idx];
            if (scanned[method_id]) continue;
            if (IsMethodMatch(
                    dex_idx, method_id,
                    return_type == size_t(-1) ? uint32_t(-2)
                                              : class_indices_[return_type][dex_idx],
                    parameter_count, parameter_shorty,
                    declaring_class == size_t(-1) ? uint32_t(-2)
                                                  : class_indices_[declaring_class][dex_idx],
                    parameter_types_ids[dex_idx], contains_parameter_types_ids[dex_idx])) {
                ScanMethod(dex_idx, method_id);
                if (find_first && !cache.empty()) break;
            }
        }
        for (const auto &setter : cache) {
            if (IsMethodMatch(dex_idx, setter,
                              return_type_id,
                              parameter_count, parameter_shorty,
                              declaring_class_id,
                              parameter_types_ids[dex_idx],
                              contains_parameter_types_ids[dex_idx])) {
                out.emplace_back(CreateMethodIndex(dex_idx, setter));
                if (find_first) return out;
            }
        }
    }
    return out;
}
std::vector<size_t> DexHelper::FindField(size_t type, const std::vector<size_t> &dex_priority,
                                         bool find_first) const {
    std::vector<size_t> out;

    if (type >= class_indices_.size()) return out;
    auto &type_ids = class_indices_[type];
    for (auto dex_idx : GetPriority(dex_priority)) {
        const auto type_id = type_ids[dex_idx];
        if (type_id == dex::kNoIndex) continue;
        for (auto &field_id : declaring_cache_[dex_idx][type_id]) {
            out.emplace_back(CreateFieldIndex(dex_idx, field_id));
            if (find_first) return out;
        }
    }
    return out;
}

bool DexHelper::IsMethodMatch(size_t dex_id, uint32_t method_id, uint32_t return_type,
                              short parameter_count, std::string_view parameter_shorty,
                              uint32_t declaring_class,
                              const std::vector<uint32_t> &parameter_types,
                              const std::vector<uint32_t> &contains_parameter_types) const {
    const auto &dex = readers_[dex_id];
    const auto &method = dex.MethodIds()[method_id];
    const auto &strs = strings_[dex_id];
    if (declaring_class != uint32_t(-2) && method.class_idx != declaring_class) return false;
    const auto &proto = dex.ProtoIds()[method.proto_idx];
    const auto &shorty = strs[proto.shorty_idx];
    if (return_type != uint32_t(-2) && proto.return_type_idx != return_type) return false;
    if (!parameter_shorty.empty() && shorty != parameter_shorty) return false;
    if (parameter_count != -1 || !parameter_types.empty() || !contains_parameter_types.empty()) {
        auto param_off = dex.ProtoIds()[method.proto_idx].parameters_off;
        const auto *params = param_off ? dex.dataPtr<dex::TypeList>(param_off) : nullptr;
        const auto params_size = params ? params->size : 0zu;
        if (parameter_count != -1 && params_size != parameter_count) return false;
        if (!parameter_types.empty()) {
            if (parameter_types.size() != params_size) return false;
            for (auto i = 0zu; i < params_size; ++i) {
                if (parameter_types[i] != uint32_t(-2) && parameter_types[i] != params->list[i].type_idx) return false;
            }
        }
        if (!contains_parameter_types.empty()) {
            for (const auto &type : contains_parameter_types) {
                bool contains = false;
                for (auto i = 0zu; i < params_size; ++i) {
                    if (type == params->list[i].type_idx) {
                        contains = true;
                        break;
                    }
                }
                if (!contains) return false;
            }
        }
    }
    return true;
}
size_t DexHelper::CreateMethodIndex(std::string_view class_name, std::string_view method_name,
                                    const std::vector<std::string_view> &params_name) const {
    std::vector<uint32_t> method_ids;
    method_ids.resize(readers_.size(), dex::kNoIndex);
    bool created = false;
    for (auto dex_idx = 0zu; dex_idx < readers_.size(); ++dex_idx) {
        const auto &strs = strings_[dex_idx];
        auto method_name_iter = std::lower_bound(strs.cbegin(), strs.cend(), method_name, utf8_less);
        if (method_name_iter == strs.cend() || *method_name_iter != method_name) continue;
        auto method_name_id = method_name_iter - strs.cbegin();
        auto class_name_iter = std::lower_bound(strs.cbegin(), strs.cend(), class_name, utf8_less);
        if (class_name_iter == strs.cend() || *class_name_iter != class_name) continue;
        auto class_name_id = class_name_iter - strs.cbegin();
        auto class_id = type_cache_[dex_idx][class_name_id];
        if (class_id == dex::kNoIndex) continue;
        auto candidates = method_cache_[dex_idx][class_id].find(method_name_id);
        if (candidates == method_cache_[dex_idx][class_id].end()) continue;
        for (const auto &method_id : candidates->second) {
            const auto &dex = readers_[dex_idx];
            auto param_off = dex.ProtoIds()[dex.MethodIds()[method_id].proto_idx].parameters_off;
            const auto *params = param_off ? dex.dataPtr<dex::TypeList>(param_off) : nullptr;
            if (params && params->size != params_name.size()) continue;
            if (!params_name.empty() && !params) continue;
            bool are_params_same = true;
            for (auto i = 0zu; i < params_name.size(); ++i) {
                if (strs[dex.TypeIds()[params->list[i].type_idx].descriptor_idx] !=
                    params_name[i]) {
                    are_params_same = false;
                    break;
                }
            }
            if (!are_params_same) continue;
            if (auto idx = rev_method_indices_[dex_idx][method_id]; idx != size_t(-1)) return idx;
            created = true;
            method_ids[dex_idx] = method_id;
        }
    }
    if (!created) return -1;
    auto index = method_indices_.size();
    for (auto dex_id = 0zu; dex_id < readers_.size(); ++dex_id) {
        auto method_id = method_ids[dex_id];
        if (method_id != dex::kNoIndex) rev_method_indices_[dex_id][method_id] = index;
    }
    method_indices_.emplace_back(std::move(method_ids));
    return index;
}

size_t DexHelper::CreateClassIndex(std::string_view class_name) const {
    std::vector<uint32_t> class_ids;
    class_ids.resize(readers_.size(), dex::kNoIndex);
    bool created = false;
    for (auto dex_idx = 0zu; dex_idx < readers_.size(); ++dex_idx) {
        const auto &strs = strings_[dex_idx];
        auto class_name_iter = std::lower_bound(strs.cbegin(), strs.cend(), class_name, utf8_less);
        if (class_name_iter == strs.cend() || *class_name_iter != class_name) continue;
        auto class_name_id = class_name_iter - strs.cbegin();
        auto class_id = type_cache_[dex_idx][class_name_id];
        if (class_id == dex::kNoIndex) continue;
        if (auto idx = rev_class_indices_[dex_idx][class_id]; idx != size_t(-1)) return idx;
        created = true;
        class_ids[dex_idx] = class_id;
    }
    if (!created) return -1;
    auto index = class_indices_.size();
    for (auto dex_id = 0zu; dex_id < readers_.size(); ++dex_id) {
        auto class_id = class_ids[dex_id];
        if (class_id != dex::kNoIndex) rev_class_indices_[dex_id][class_id] = index;
    }
    class_indices_.emplace_back(std::move(class_ids));
    return index;
}

size_t DexHelper::CreateFieldIndex(std::string_view class_name, std::string_view field_name) const {
    std::vector<uint32_t> field_ids;
    field_ids.resize(readers_.size(), dex::kNoIndex);

    bool created = false;
    for (auto dex_idx = 0zu; dex_idx < readers_.size(); ++dex_idx) {
        const auto &strs = strings_[dex_idx];
        auto class_name_iter = std::lower_bound(strs.cbegin(), strs.cend(), class_name, utf8_less);
        if (class_name_iter == strs.cend() || *class_name_iter != class_name) continue;
        auto class_name_id = class_name_iter - strs.cbegin();
        auto field_name_iter = std::lower_bound(strs.cbegin(), strs.cend(), field_name, utf8_less);
        if (field_name_iter == strs.cend() || *field_name_iter != field_name) continue;
        auto field_name_id = field_name_iter - strs.cbegin();
        auto class_id = type_cache_[dex_idx][class_name_id];
        if (class_id == dex::kNoIndex) continue;
        auto iter = field_cache_[dex_idx][class_id].find(field_name_id);
        if (iter == field_cache_[dex_idx][class_id].end()) continue;
        auto field_id = iter->second;
        if (auto idx = rev_field_indices_[dex_idx][field_id]; idx != size_t(-1)) return idx;
        created = true;
        field_ids[dex_idx] = field_id;
    }
    if (!created) return -1;
    auto index = field_indices_.size();
    for (auto dex_id = 0zu; dex_id < readers_.size(); ++dex_id) {
        auto field_id = field_ids[dex_id];
        if (field_id != dex::kNoIndex) rev_field_indices_[dex_id][field_id] = index;
    }
    field_indices_.emplace_back(std::move(field_ids));
    return index;
}

size_t DexHelper::CreateMethodIndex(size_t dex_idx, uint32_t method_id) const {
    const auto &dex = readers_[dex_idx];
    const auto &strs = strings_[dex_idx];
    const auto &method = dex.MethodIds()[method_id];
    auto param_off = dex.ProtoIds()[dex.MethodIds()[method_id].proto_idx].parameters_off;
    const auto *params = param_off ? dex.dataPtr<dex::TypeList>(param_off) : nullptr;
    std::vector<std::string_view> param_names;
    if (params) {
        param_names.reserve(params->size);
        for (auto i = 0zu; i < params->size; ++i) {
            param_names.emplace_back(strs[dex.TypeIds()[params->list[i].type_idx].descriptor_idx]);
        }
    }
    return CreateMethodIndex(strs[dex.TypeIds()[method.class_idx].descriptor_idx],
                             strs[method.name_idx], param_names);
}

size_t DexHelper::CreateClassIndex(size_t dex_idx, uint32_t class_id) const {
    const auto &dex = readers_[dex_idx];
    const auto &strs = strings_[dex_idx];
    return CreateClassIndex(strs[dex.TypeIds()[class_id].descriptor_idx]);
}

size_t DexHelper::CreateFieldIndex(size_t dex_idx, uint32_t field_id) const {
    const auto &dex = readers_[dex_idx];
    const auto &strs = strings_[dex_idx];
    const auto &field = dex.FieldIds()[field_id];
    return CreateFieldIndex(strs[dex.TypeIds()[field.class_idx].descriptor_idx],
                            strs[field.name_idx]);
}

auto DexHelper::DecodeClass(size_t class_idx) const -> Class {
    if (class_idx >= class_indices_.size()) return {};
    auto &class_ids = class_indices_[class_idx];
    for (auto dex_idx = 0zu; dex_idx < readers_.size(); ++dex_idx) {
        auto class_id = class_ids[dex_idx];
        if (class_id == dex::kNoIndex) continue;
        return {
            .name = strings_[dex_idx][readers_[dex_idx].TypeIds()[class_id].descriptor_idx],
        };
    }
    return {};
}

auto DexHelper::DecodeField(size_t field_idx) const -> Field {
    if (field_idx >= field_indices_.size()) return {};
    auto &field_ids = field_indices_[field_idx];
    for (auto dex_idx = 0zu; dex_idx < readers_.size(); ++dex_idx) {
        auto field_id = field_ids[dex_idx];
        if (field_id == dex::kNoIndex) continue;
        const auto &dex = readers_[dex_idx];
        const auto &field = dex.FieldIds()[field_id];
        const auto &strs = strings_[dex_idx];
        return {
            .declaring_class =
                {
                    .name = strs[dex.TypeIds()[field.class_idx].descriptor_idx],
                },
            .type = {.name = strs[dex.TypeIds()[field.type_idx].descriptor_idx]},
            .name = strings_[dex_idx][field.name_idx],
        };
    }
    return {};
}

auto DexHelper::DecodeMethod(size_t method_idx) const -> Method {
    if (method_idx >= method_indices_.size()) return {};
    auto &method_ids = method_indices_[method_idx];
    for (auto dex_idx = 0zu; dex_idx < readers_.size(); ++dex_idx) {
        auto method_id = method_ids[dex_idx];
        if (method_id == dex::kNoIndex) continue;
        const auto &dex = readers_[dex_idx];
        const auto &method = dex.MethodIds()[method_id];
        const auto &strs = strings_[dex_idx];
        std::vector<Class> parameters;
        auto param_off = dex.ProtoIds()[dex.MethodIds()[method_id].proto_idx].parameters_off;
        const auto *params = param_off ? dex.dataPtr<dex::TypeList>(param_off) : nullptr;
        auto params_size = params ? params->size : 0zu;
        for (auto i = 0zu; i < params_size; ++i) {
            parameters.emplace_back(Class{
                .name = strs[dex.TypeIds()[params->list[i].type_idx].descriptor_idx],
            });
        }
        return {.declaring_class =
                    {
                        .name = strs[dex.TypeIds()[method.class_idx].descriptor_idx],
                    },
                .name = strs[method.name_idx],
                .parameters = std::move(parameters),
                .return_type = {
                    .name = strs[dex.TypeIds()[dex.ProtoIds()[method.proto_idx].return_type_idx]
                                     .descriptor_idx]}};
    }
    return {};
}

std::vector<size_t> DexHelper::GetPriority(const std::vector<size_t> &priority) const {
    std::vector<size_t> out;
    if (priority.empty()) {
        for (auto i = 0zu; i < readers_.size(); ++i) {
            out.emplace_back(i);
        }
    } else {
        for (const auto &i : priority) {
            if (i < readers_.size()) {
                out.emplace_back(i);
            }
        }
    }
    return out;
}
