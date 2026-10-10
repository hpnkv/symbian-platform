/*
 * Copyright 2026 The A11 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file
 * @brief A11's JSON/MessagePack codec, adapted to compile without exceptions.
 *
 * Parsing uses allow_exceptions=false. Serialization preflights string values
 * and object keys. Invalid input becomes an Abseil status; diagnostic reparsing
 * with exceptions is omitted. See the archived ~/.symbian-dev/a11-status.md
 * for the recorded changes.
 */

#ifndef SYMBIAN_STATUS_JSON_CODEC_H_
#define SYMBIAN_STATUS_JSON_CODEC_H_

#include <string>
#include <string_view>

#include <absl/base/nullability.h>
#include <absl/status/statusor.h>
#include <nlohmann/json.hpp>

namespace symbian {

/**
 * @brief Parses JSON text, or explains why it is not JSON.
 *
 * @param encoded JSON text to parse.
 * @param what Names the document in the error, e.g. "WireMessage JSON".
 */
absl::StatusOr<nlohmann::json> ParseJson(std::string_view encoded,
                                         std::string_view what);

/**
 * @brief Whether @p text is valid UTF-8.
 *
 * Public because the answer has to be available *before* nlohmann is asked. See
 * DumpJson for why asking nlohmann is not safe.
 *
 * Strict by the definition every other A11 language's string type enforces,
 * which is stricter than "the bytes are shaped like UTF-8": overlong encodings,
 * surrogate halves and anything above U+10FFFF are all rejected. Python's
 * `bytes.decode("utf-8")`, Kotlin's `String(bytes)` and JavaScript's
 * `TextDecoder` with `fatal` all refuse them, so a chunk carrying them is one a
 * peer cannot read -- and a value this side refuses to write is a much better
 * outcome than a session that dies one hop away.
 */
bool IsValidUtf8(std::string_view text);

/**
 * @brief The first string inside @p value that is not valid UTF-8, or nullptr.
 *
 * The strings that come from outside are not only at the top level: a response
 * header's value and a directory entry's name are both fields of a record, and
 * both are exactly where something outside this process can put arbitrary
 * bytes.
 *
 * Iterative, not recursive: this runs on whatever fiber is serializing, and
 * A11's fiber stacks are fixed and small, so the depth of the document must not
 * decide how much stack the check needs.
 */
const nlohmann::json* absl_nullable FindUnencodableString(
    const nlohmann::json& value);

/**
 * @brief Serializes @p value, rejecting strings that are not valid UTF-8.
 *
 * Strict on purpose: JSON is defined over text, and a chunk holding arbitrary
 * bytes has to be encoded (base64, as `data/json.cc` does) rather than smuggled
 * into a string field where a peer's parser would reject it. This is the one
 * caller of nlohmann that wants the error rather than a replacement character.
 *
 * String values and object keys are checked before invoking nlohmann's
 * strict serializer, including in native tests compiled without exceptions.
 */
absl::StatusOr<std::string> DumpJson(const nlohmann::json& value,
                                     std::string_view what);

/**
 * @brief Serializes @p value, replacing anything that is not valid UTF-8.
 *
 * For a log line or a span attribute, where a lost byte is better than a lost
 * message and there is no peer to reject it.
 */
std::string DumpJsonLossy(const nlohmann::json& value);

/** @brief Encodes @p value as MessagePack, or says why it cannot be. */
absl::StatusOr<std::string> PackMsgpack(const nlohmann::json& value,
                                        std::string_view what);

/** @brief Decodes MessagePack bytes, or explains why they are not. */
absl::StatusOr<nlohmann::json> UnpackMsgpack(std::string_view encoded,
                                             std::string_view what);

}  // namespace symbian

#endif  // SYMBIAN_STATUS_JSON_CODEC_H_
