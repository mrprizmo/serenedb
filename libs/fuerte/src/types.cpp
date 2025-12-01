////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Christoph Uhde
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////
#include <fuerte/types.h>

#include <algorithm>
#include <stdexcept>

namespace sdb::fuerte {

std::string StatusCodeToString(StatusCode status_code) {
  switch (status_code) {
    case kStatusUndefined:
      return "0 Undefined";
    case kStatusOk:
      return "200 OK";
    case kStatusCreated:
      return "201 Created";
    case kStatusAccepted:
      return "202 Accepted";
    case kStatusPartial:
      return "203 Partial";
    case kStatusNoContent:
      return "204 No Content";
    case kStatusBadRequest:
      return "400 Bad Request";
    case kStatusUnauthorized:
      return "401 Unauthorized";
    case kStatusForbidden:
      return "403 Forbidden";
    case kStatusNotFound:
      return "404 Not Found";
    case kStatusMethodNotAllowed:
      return "405 Method Not Allowed";
    case kStatusNotAcceptable:
      return "406 Not Acceptable";
    case kStatusConflict:
      return "409 Conflict";
    case kStatusPreconditionFailed:
      return "412 Precondition Failed";
    case kStatusInternalError:
      return "500 Internal Error";
    case kStatusServiceUnavailable:
      return "503 Unavailable";
    case kStatusVersionNotSupported:
      return "505 Version Not Supported";
    default:
      if (100 <= status_code && status_code < 200) {
        return std::to_string(status_code) + " Unknown informational response";
      } else if (200 <= status_code && status_code < 300) {
        return std::to_string(status_code) + " Unknown successful response";
      } else if (300 <= status_code && status_code < 400) {
        return std::to_string(status_code) + " Unknown redirect";
      } else if (400 <= status_code && status_code < 500) {
        return std::to_string(status_code) + " Unknown client error";
      } else if (500 <= status_code && status_code < 600) {
        return std::to_string(status_code) + " Unknown server error";
      } else {
        return std::to_string(status_code) + " Unknown or invalid status code";
      }
  }
}

bool StatusIsSuccess(StatusCode status_code) {
  return 200 <= status_code && status_code < 300;
}

std::string ToString(RestVerb type) {
  switch (type) {
    case RestVerb::Illegal:
      return "illegal";

    case RestVerb::Delete:
      return "DELETE";

    case RestVerb::Get:
      return "GET";

    case RestVerb::Post:
      return "POST";

    case RestVerb::Put:
      return "PUT";

    case RestVerb::Head:
      return "HEAD";

    case RestVerb::Patch:
      return "PATCH";

    case RestVerb::Options:
      return "OPTIONS";
  }

  return "undefined";
}

RestVerb FromString(std::string_view type) {
  if (type == "DELETE") {
    return RestVerb::Delete;
  } else if (type == "GET") {
    return RestVerb::Get;
  } else if (type == "POST") {
    return RestVerb::Post;
  } else if (type == "PUT") {
    return RestVerb::Put;
  } else if (type == "HEAD") {
    return RestVerb::Head;
  } else if (type == "PATCH") {
    return RestVerb::Patch;
  } else if (type == "OPTIONS") {
    return RestVerb::Options;
  }

  return RestVerb::Illegal;
}

RestVerb FromString(const std::string& type) {
  return FromString(std::string_view(type.data(), type.size()));
}

MessageType IntToMessageType(int integral) {
  switch (integral) {
    case 1:
      return MessageType::Request;
    case 2:
      return MessageType::Response;
    case 1000:
      return MessageType::Authentication;
    default:
      break;
  }
  return MessageType::Undefined;
}

std::string ToString(MessageType type) {
  switch (type) {
    case MessageType::Undefined:
      return "undefined";

    case MessageType::Request:
      return "request";

    case MessageType::Response:
      return "response";

    case MessageType::Authentication:
      return "authentication";
  }

  return "undefined";
}

std::string ToString(SocketType type) {
  switch (type) {
    case SocketType::Undefined:
      return "undefined";

    case SocketType::Tcp:
      return "tcp";

    case SocketType::Ssl:
      return "ssl";

    case SocketType::Unix:
      return "unix";
  }

  return "undefined";
}

std::string ToString(ProtocolType type) {
  switch (type) {
    case ProtocolType::Undefined:
      return "undefined";

    case ProtocolType::Http:
    case ProtocolType::Http2:
      return "http";
  }

  return "undefined";
}

const std::string kFuContentTypeUnset("unset");
const std::string kFuContentTypeVPack("application/x-vpack");
const std::string kFuContentTypeJson("application/json");
const std::string kFuContentTypeHtml("text/html");
const std::string kFuContentTypeText("text/plain");
const std::string kFuContentTypeDump("application/x-serene-dump");
const std::string kFuContentTypeBatchpart("application/x-serene-batchpart");
const std::string kFuContentTypeFormdata("multipart/form-data");

ContentType ToContentType(std::string_view val) {
  if (val.empty()) {
    return ContentType::Unset;
  } else if (val.compare(0, kFuContentTypeUnset.size(), kFuContentTypeUnset) ==
             0) {
    return ContentType::Unset;
  } else if (val.compare(0, kFuContentTypeVPack.size(), kFuContentTypeVPack) ==
             0) {
    return ContentType::VPack;
  } else if (val.compare(0, kFuContentTypeJson.size(), kFuContentTypeJson) ==
             0) {
    return ContentType::Json;
  } else if (val.compare(0, kFuContentTypeHtml.size(), kFuContentTypeHtml) ==
             0) {
    return ContentType::Html;
  } else if (val.compare(0, kFuContentTypeText.size(), kFuContentTypeText) ==
             0) {
    return ContentType::Text;
  } else if (val.compare(0, kFuContentTypeDump.size(), kFuContentTypeDump) ==
             0) {
    return ContentType::Dump;
  } else if (val.compare(0, kFuContentTypeBatchpart.size(),
                         kFuContentTypeBatchpart) == 0) {
    return ContentType::BatchPart;
  } else if (val.compare(0, kFuContentTypeFormdata.size(),
                         kFuContentTypeFormdata) == 0) {
    return ContentType::FormData;
  }

  return ContentType::Custom;
}

std::string ToString(ContentType type) {
  switch (type) {
    case ContentType::Unset:
      return kFuContentTypeUnset;

    case ContentType::VPack:
      return kFuContentTypeVPack;

    case ContentType::Json:
      return kFuContentTypeJson;

    case ContentType::Html:
      return kFuContentTypeHtml;

    case ContentType::Text:
      return kFuContentTypeText;

    case ContentType::Dump:
      return kFuContentTypeDump;

    case ContentType::BatchPart:
      return kFuContentTypeBatchpart;

    case ContentType::FormData:
      return kFuContentTypeFormdata;

    case ContentType::Custom:
      throw std::logic_error(
        "custom content type could take different "
        "values and is therefore not convertible to string");
  }

  throw std::logic_error("unknown content type");
}

const std::string kFuContentEncodingIdentity("identity");
const std::string kFuContentEncodingDeflate("deflate");
const std::string kFuContentEncodingGzip("gzip");
const std::string kFuContentEncodingLz4("x-serene-lz4");

ContentEncoding ToContentEncoding(std::string_view val) {
  if (val.empty()) {
    return ContentEncoding::Identity;
  }

  size_t pos = 0;
  while (pos < val.size()) {
    std::string_view current;
    // split multiple specified encodings at ','
    size_t next = val.find(',', pos);
    if (next == std::string::npos) {
      current = {val.data() + pos, val.size() - pos};
      pos = val.size();
    } else {
      current = {val.data() + pos, next - pos};
      pos = next + 1;
    }

    // each encoding can have a "quality"/weight value attached.
    // strip it.
    next = current.find(';');
    if (next != std::string_view::npos) {
      current = current.substr(0, next);
    }
    // ltrim the result value
    while (!current.empty() && current.front() == ' ') {
      current = current.substr(1);
    }
    // rtrim the result value
    while (!current.empty() && current.back() == ' ') {
      current = current.substr(0, current.size() - 1);
    }

    if (current == kFuContentEncodingLz4) {
      return ContentEncoding::Lz4;
    }
    if (current == kFuContentEncodingGzip) {
      return ContentEncoding::Gzip;
    }
    if (current == kFuContentEncodingDeflate) {
      return ContentEncoding::Deflate;
    }
    if (current == kFuContentEncodingIdentity) {
      return ContentEncoding::Identity;
    }
  }

  return ContentEncoding::Custom;
}

std::string ToString(ContentEncoding type) {
  switch (type) {
    case ContentEncoding::Deflate:
      return kFuContentEncodingDeflate;
    case ContentEncoding::Gzip:
      return kFuContentEncodingGzip;
    case ContentEncoding::Lz4:
      return kFuContentEncodingLz4;
    case ContentEncoding::Custom:
      throw std::logic_error(
        "custom content encoding could take different "
        "values and is therefore not convertible to string");
    case ContentEncoding::Identity:
    default:
      return kFuContentEncodingIdentity;
  }
}

std::string ToString(AuthenticationType type) {
  switch (type) {
    case AuthenticationType::None:
      return "none";
    case AuthenticationType::Basic:
      return "basic";
    case AuthenticationType::Jwt:
      return "jwt";
  }
  return "unknown";
}

std::string ToString(Error error) {
  switch (error) {
    case Error::NoError:
      return "No Error";
    case Error::CouldNotConnect:
      return "Unable to connect";
    case Error::CloseRequested:
      return "Peer requested connection close";
    case Error::ConnectionClosed:
      return "Connection closed";
    case Error::RequestTimeout:
      return "Request timeout";
    case Error::QueueCapacityExceeded:
      return "Request queue capacity exceeded";
    case Error::ReadError:
      return "Error while reading";
    case Error::WriteError:
      return "Error while writing";
    case Error::ConnectionCanceled:
      return "Connection was locally canceled";

    case Error::ProtocolError:
      return "Error: invalid server response";
  }
  return "unkown error";
}

}  // namespace sdb::fuerte
