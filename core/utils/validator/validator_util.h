#pragma once

#include <google/protobuf/message.h>
#include <butil/logging.h>
#include <regex>
#include <algorithm>
#include <sstream>
#include "validator.pb.h"

namespace server {
namespace utils {

using namespace google::protobuf;
using namespace validator;

/** 不知道为什么protobuf对ValidateRules中float和double两个字段生成的字段名会加个后缀_(其他字段没有), 为了在宏里面统一处理加了下面两个定义 */
typedef float float_;
typedef double double_;

/**
 * 数值校验器(适用于int32、int64、uint32、uint64、float、double)
 * 支持大于、大于等于、小于、小于等于、in、not_in校验
*/
#define NumericalValidator(pb_cpptype, method_type, value_type)                                    \
    case google::protobuf::FieldDescriptor::CPPTYPE_##pb_cpptype: {                                \
        if (validate_rules.has_##value_type()) {                                                   \
            const method_type##Rule& rule = validate_rules.value_type();                           \
            value_type value              = reflection->Get##method_type(message, field);          \
            if ((rule.lt_rule_case() && value >= rule.lt()) ||                                     \
                (rule.lte_rule_case() && value > rule.lte()) ||                                    \
                (rule.gt_rule_case() && value <= rule.gt()) ||                                     \
                (rule.gte_rule_case() && value < rule.gte())) {                                    \
                std::ostringstream os;                                                             \
                os << field->full_name() << " value out of range.";                                \
                return {false, os.str()};                                                          \
            }                                                                                      \
            if ((!rule.in().empty() &&                                                             \
                 std::find(rule.in().begin(), rule.in().end(), value) == rule.in().end()) ||       \
                (!rule.not_in().empty() &&                                                         \
                 std::find(rule.not_in().begin(), rule.not_in().end(), value) !=                   \
                     rule.not_in().end())) {                                                       \
                std::ostringstream os;                                                             \
                os << field->full_name() << " value not allowed.";                                 \
                return {false, os.str()};                                                          \
            }                                                                                      \
        }                                                                                          \
        break;                                                                                     \
    }

/**
 * 字符串校验器(string)
 * 支持字符串非空校验、最短(最长)长度校验、正则匹配校验
*/
#define StringValidator(pb_cpptype, method_type, value_type)                                       \
    case google::protobuf::FieldDescriptor::CPPTYPE_##pb_cpptype: {                                \
        if (validate_rules.has_##value_type()) {                                                   \
            const method_type##Rule& rule = validate_rules.value_type();                           \
            const value_type& value       = reflection->Get##method_type(message, field);          \
            if (rule.not_empty() && value.empty()) {                                               \
                std::ostringstream os;                                                             \
                os << field->full_name() << " can not be empty.";                                  \
                return {false, os.str()};                                                          \
            }                                                                                      \
            if ((rule.min_len_rule_case() && value.length() < rule.min_len()) ||                   \
                (rule.max_len_rule_case() && value.length() > rule.max_len())) {                   \
                std::ostringstream os;                                                             \
                os << field->full_name() << " length out of range.";                               \
                return {false, os.str()};                                                          \
            }                                                                                      \
            if (!value.empty() && !rule.regex_pattern().empty()) {                                 \
                std::regex ex(rule.regex_pattern());                                               \
                if (!regex_match(value, ex)) {                                                     \
                    std::ostringstream os;                                                         \
                    os << field->full_name() << " format invalid.";                                \
                    return {false, os.str()};                                                      \
                }                                                                                  \
            }                                                                                      \
        }                                                                                          \
        break;                                                                                     \
    }

/**
 * 枚举校验器(enum)
 * 仅支持in校验
*/
#define EnumValidator(pb_cpptype, method_type, value_type)                                          \
    case google::protobuf::FieldDescriptor::CPPTYPE_##pb_cpptype: {                                 \
        if (validate_rules.has_##value_type()) {                                                    \
            const method_type##Rule& rule = validate_rules.value_type();                            \
            int value                     = reflection->Get##method_type(message, field)->number(); \
            if (!rule.in().empty() &&                                                               \
                std::find(rule.in().begin(), rule.in().end(), value) == rule.in().end()) {          \
                std::ostringstream os;                                                              \
                os << field->full_name() << " value not allowed.";                                  \
                return {false, os.str()};                                                           \
            }                                                                                       \
        }                                                                                           \
        break;                                                                                      \
    }

/**
 * 数组校验器(array)
 * 支持数组非空校验、最短(最长)长度校验以及Message结构体元素递归校验
*/
#define ArrayValidator()                                                                           \
    uint32 arr_len = (uint32)reflection->FieldSize(message, field);                                \
    if (validate_rules.has_array()) {                                                              \
        const ArrayRule& rule = validate_rules.array();                                            \
        if (rule.not_empty() && arr_len == 0) {                                                    \
            std::ostringstream os;                                                                 \
            os << field->full_name() << " can not be empty.";                                      \
            return {false, os.str()};                                                              \
        }                                                                                          \
        if ((rule.min_len() != 0 && arr_len < rule.min_len()) ||                                   \
            (rule.max_len() != 0 && arr_len > rule.max_len())) {                                   \
            std::ostringstream os;                                                                 \
            os << field->full_name() << " length out of range.";                                   \
            return {false, os.str()};                                                              \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    /* 如果数组元素是Message结构体类型，递归校验每个元素 */                   \
    if (field_type == FieldDescriptor::CPPTYPE_MESSAGE) {                                          \
        for (uint32 i = 0; i < arr_len; i++) {                                                     \
            const Message& sub_message = reflection->GetRepeatedMessage(message, field, i);        \
            ValidateResult&& result    = Validate(sub_message);                                    \
            if (!result.is_valid) {                                                                \
                return result;                                                                     \
            }                                                                                      \
        }                                                                                          \
    }

/**
 * 结构体校验器(Message)
 * (递归校验)
*/
#define MessageValidator()                                                                         \
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {                                     \
        const Message& sub_message = reflection->GetMessage(message, field);                       \
        ValidateResult&& result    = Validate(sub_message);                                        \
        if (!result.is_valid) {                                                                    \
            return result;                                                                         \
        }                                                                                          \
        break;                                                                                     \
    }

class ValidatorUtil {
public:
    struct ValidateResult {
        bool is_valid;
        std::string msg;
    };

    static ValidateResult Validate(const Message& message) {
        const Descriptor* descriptor = message.GetDescriptor();
        const Reflection* reflection = message.GetReflection();

        for (int i = 0; i < descriptor->field_count(); i++) {
            const FieldDescriptor* field        = descriptor->field(i);
            FieldDescriptor::CppType field_type = field->cpp_type();
            const ValidateRules& validate_rules = field->options().GetExtension(validator::Rule);

            if (field->is_repeated()) {
                // 数组类型校验
                ArrayValidator();
            } else {
                // 非数组类型，直接调用对应类型校验器
                switch (field_type) {
                    NumericalValidator(INT32, Int32, int32);
                    NumericalValidator(INT64, Int64, int64);
                    NumericalValidator(UINT32, UInt32, uint32);
                    NumericalValidator(UINT64, UInt64, uint64);
                    NumericalValidator(FLOAT, Float, float_);
                    NumericalValidator(DOUBLE, Double, double_);
                    StringValidator(STRING, String, string);
                    EnumValidator(ENUM, Enum, enum_);
                    MessageValidator();
                    default:
                        break;
                }
            }
        }
        return {true, ""};
    }
};

} // namespace utils
} // namespace server