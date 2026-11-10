#include "core_json.h"

// ============================================================================
// HELPER: Trim in-place (mismo criterio que Str_AllTrim de core_string.h,
// mantenido local y estático acá para no crear una dependencia cruzada entre
// módulos "core_*" independientes).
// ============================================================================
static void Json_AllTrim(char *str) 
{
    if (!str) return;
    if (*str == '\0') return;

    char *start = str;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;

    if (*start == '\0')
    {
        *str = '\0';
        return;
    }

    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        end--;
    *(end + 1) = '\0';

    if (start != str)
        memmove(str, start, strlen(start) + 1);
}

// ============================================================================
// HELPER INTERNO: Navega un path JSON tipo "results[0].shipping.id"
// ============================================================================
static struct json_value_s *Json_NavigatePath(struct json_value_s *root, const char *path)
{
    if (!root || !path) return NULL;

    size_t len = strlen(path);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, path, len + 1);

    struct json_value_s *node = root;
    char *saveptr = NULL;
    char *token = strtok_s(buf, ".", &saveptr);

    while (token && node)
    {
        Json_AllTrim(token);
        char *bracket = strchr(token, '[');

        if (bracket)
        {
            *bracket = '\0';
            Json_AllTrim(token);
            int idx = atoi(bracket + 1);

            struct json_object_s *obj = json_value_as_object(node);
            node = NULL;
            if (obj)
            {
                struct json_object_element_s *elem = obj->start;
                while (elem)
                {
                    char keyCopy[256];
                    strncpy(keyCopy, elem->name->string, sizeof(keyCopy) - 1);
                    keyCopy[sizeof(keyCopy) - 1] = '\0';
                    Json_AllTrim(keyCopy);

                    if (strcmp(keyCopy, token) == 0)
                    {
                        node = elem->value;
                        break;
                    }
                    elem = elem->next;
                }
            }

            if (node && idx >= 0)
            {
                struct json_array_s *arr = json_value_as_array(node);
                node = NULL;
                if (arr)
                {
                    struct json_array_element_s *aelem = arr->start;
                    for (int i = 0; aelem && i < idx; i++)
                        aelem = aelem->next;
                    if (aelem)
                        node = aelem->value;
                }
            }
        }
        else
        {
            struct json_object_s *obj = json_value_as_object(node);
            node = NULL;
            if (obj)
            {
                struct json_object_element_s *elem = obj->start;
                while (elem)
                {
                    size_t keyLen = elem->name->string_size;
                    if (keyLen >= 512) keyLen = 511;  // Límite seguro

                    char keyCopy[512];
                    memcpy(keyCopy, elem->name->string, keyLen);
                    keyCopy[keyLen] = '\0';
                    Json_AllTrim(keyCopy);
                   

                    if (strcmp(keyCopy, token) == 0)
                    {
                        node = elem->value;
                        break;
                    }
                    elem = elem->next;
                }
            }
        }
        token = strtok_s(NULL, ".", &saveptr);
    }

    free(buf);
    return node;
}
 
typedef struct
{
    struct json_value_s *root;
    struct json_value_s *node;
} JsonParseResult;

static JsonParseResult Json_ParseAndNavigateNode(const char *json, const char *path)
{
    JsonParseResult r = {0};   
    if (!json || !path) return r;
    json = StrCleanSpaces(json);
    //    
    r.root = json_parse(json, strlen(json));
    if (r.root)
        r.node = Json_NavigatePath(r.root, path);

    return r;
}
 
static void Json_Cleanup(JsonParseResult *r)
{
    if (!r) return;
    if (r->root) free(r->root);
    r->root = NULL;
    r->node = NULL;
}

static size_t Json_WriteString(const char *str, size_t len, char *buf, size_t bufSize)
{
    if (!buf || bufSize == 0) return 0;

    size_t pos = 0;
    if (pos < bufSize) buf[pos++] = '"';

    for (size_t i = 0; i < len && pos < bufSize - 1; i++)
    {
        char c = str[i];
        if (c == '"' || c == '\\')
        {
            if (pos < bufSize - 1) buf[pos++] = '\\';
        }
        if (pos < bufSize) buf[pos++] = c;
    }
    if (pos < bufSize) buf[pos++] = '"';
    return pos;
}

static size_t Json_SerializeNode(struct json_value_s *node, char *buf, size_t bufSize)
{
    if (!node || !buf || bufSize == 0) return 0;

    size_t pos = 0;

    switch (node->type)
    {
        case json_type_string:
        {
            struct json_string_s *s = (struct json_string_s *)node->payload;
            pos = Json_WriteString(s->string, s->string_size, buf, bufSize);
            break;
        }
        case json_type_number:
        {
            struct json_number_s *n = (struct json_number_s *)node->payload;
            size_t nlen = strlen(n->number);
            if (nlen >= bufSize) nlen = bufSize - 1;
            memcpy(buf, n->number, nlen);
            buf[nlen] = '\0';
            pos = nlen;
            break;
        }
        case json_type_true:
            if (bufSize > 4) { memcpy(buf, "true", 4); pos = 4; }
            break;
        case json_type_false:
            if (bufSize > 5) { memcpy(buf, "false", 5); pos = 5; }
            break;
        case json_type_null:
            if (bufSize > 4) { memcpy(buf, "null", 4); pos = 4; }
            break;
        case json_type_array:
        {
            struct json_array_s *arr = (struct json_array_s *)node->payload;
            if (pos < bufSize) buf[pos++] = '[';

            struct json_array_element_s *elem = arr->start;
            while (elem && pos < bufSize - 1)
            {
                if (elem != arr->start && pos < bufSize) buf[pos++] = ',';

                size_t remaining = bufSize - pos;
                if (remaining > 0)
                {
                    size_t written = Json_SerializeNode(elem->value, buf + pos, remaining);
                    pos += written;
                }
                elem = elem->next;
            }
            if (pos < bufSize) buf[pos++] = ']';
            break;
        }
        case json_type_object:
        {
            struct json_object_s *obj = (struct json_object_s *)node->payload;
            if (pos < bufSize) buf[pos++] = '{';

            struct json_object_element_s *elem = obj->start;
            while (elem && pos < bufSize - 1)
            {
                if (elem != obj->start && pos < bufSize) buf[pos++] = ',';

                size_t keyWritten = Json_WriteString(elem->name->string,
                                                      elem->name->string_size,
                                                      buf + pos,
                                                      bufSize - pos);
                pos += keyWritten;

                if (pos < bufSize) buf[pos++] = ':';

                size_t remaining = bufSize - pos;
                if (remaining > 0)
                {
                    size_t written = Json_SerializeNode(elem->value, buf + pos, remaining);
                    pos += written;
                }
                elem = elem->next;
            }
            if (pos < bufSize) buf[pos++] = '}';
            break;
        }
    }

    return pos;
}
 
char *JsonGetString(const char *json, const char *path)
{
    char *result = NULL;

    if (!json || !path) return  _strdup("");

    JsonParseResult r = Json_ParseAndNavigateNode(json, path);

    if (r.node)
    {
        struct json_string_s *str = json_value_as_string(r.node);
        if (str && str->string)
            result = _strdup(str->string);
    }

    Json_Cleanup(&r);
    return result ? result : _strdup("");
}

double JsonGetDouble(const char *json, const char *path)
{
    double result = 0.0;
    if (!json || !path) return result;

    JsonParseResult r = Json_ParseAndNavigateNode(json, path);


    if (r.node)
    {
        struct json_number_s *num = json_value_as_number(r.node);
        if (num && num->number)
            result = atof(num->number);
    }

    Json_Cleanup(&r);
    return result;
} 
char *JsonGetInt64(const char *json, const char *path)
{ 
    if (!json || !path) return  _strdup("");
     
    JsonParseResult r = Json_ParseAndNavigateNode(json, path);
     __int64 result = 0;
    if (r.node)
    {
        struct json_number_s *num = json_value_as_number(r.node);
        if (num && num->number)
            result = _atoi64(num->number);
    }

    Json_Cleanup(&r);

    char *buf = (char *)malloc(32);
    if (buf)
        _i64toa(result, buf, 10);
    return buf;  
}

long JsonGetInt(const char *json, const char *path)
{
    long result = 0;
    if (!json || !path) return result;

    JsonParseResult r = Json_ParseAndNavigateNode(json, path);
    
    if (r.node)
    {
        struct json_number_s *num = json_value_as_number(r.node);
        if (num && num->number)
            result = atol(num->number);
    }

    Json_Cleanup(&r);
    return result;
}

int JsonGetBool(const char *json, const char *path)
{
    int result = 0;
    if (!json || !path) return result;

    JsonParseResult r = Json_ParseAndNavigateNode(json, path);

    if (r.node)
        result = json_value_is_true(r.node) ? 1 : 0;

    Json_Cleanup(&r);
    return result;
}

int JsonHasValue(const char *json, const char *path)
{
    int result = 0;
    if (!json || !path) return result;

    JsonParseResult r = Json_ParseAndNavigateNode(json, path);
    result = (r.node != NULL && !json_value_is_null(r.node)) ? 1 : 0;
    Json_Cleanup(&r);
    return result;
}

int JsonArrayCount(const char *json, const char *path)
{
    int result = 0;
    if (!json || !path) return result;

    JsonParseResult r = Json_ParseAndNavigateNode(json, path);
  
    if (r.node)
    {
        struct json_array_s *arr = json_value_as_array(r.node);
        if (arr) result = (int)arr->length;
    }

    Json_Cleanup(&r);
    return result;
}
 
const char *JsonGetType(const char *json, const char *path)
{
    const char *result = "missing";
    if (!json || !path) {
        result = "null";
        return result;
    }

    JsonParseResult r = Json_ParseAndNavigateNode(json, path);

    if (r.node)
    {
        if (json_value_is_null(r.node))
            result = "null";
        else
        {
            switch (r.node->type)
            {
                case json_type_string: result = "string"; break;
                case json_type_number: result = "number"; break;
                case json_type_object: result = "object"; break;
                case json_type_array:  result = "array";  break;
                case json_type_true:
                case json_type_false:  result = "bool";   break;
                default:                result = "unknown"; break;
            }
        }
    }

    Json_Cleanup(&r);
    return result;
} 
char *JsonExtractSubNode(const char *json, const char *path)
{
    char *result = NULL;
    if (!json || !path) return result;

    JsonParseResult r = Json_ParseAndNavigateNode(json, path);

    if (r.node)
    {
        size_t bufSize = 65536; // 64KB; el contenido se trunca si excede esto
        char *tempBuf = (char *)malloc(bufSize);
        if (tempBuf)
        {
            size_t written = Json_SerializeNode(r.node, tempBuf, bufSize - 1);
            tempBuf[written] = '\0';
            result = tempBuf; // ownership pasa al caller, NO se libera acá
        }
    }

    Json_Cleanup(&r);
    return result ? result : _strdup("");
}

DataVariant JsonGetAny(const char *json, const char *path)
{ 
    DataVariant res;
    res.type = DATA_TYPE_EMPTY;
    
    if (!json || !path) return res;

    JsonParseResult r = Json_ParseAndNavigateNode(json, path);

    if (!r.node || json_value_is_null(r.node))
    {
        res.type = DATA_TYPE_CHARACTER;
        res.value.cVal = _strdup("");
        Json_Cleanup(&r);
        return res;
    }

    switch (r.node->type)
    {
        case json_type_string:
        {
            struct json_string_s *str = json_value_as_string(r.node);
            res.type = DATA_TYPE_CHARACTER;
            res.value.cVal = _strdup(str && str->string ? str->string : "");
            break;
        }
        case json_type_number:
        {
            struct json_number_s *num = json_value_as_number(r.node);
            if (num && num->number)
            {
                const char *s = num->number;
                int isInteger = 1;
                while (*s)
                {
                    if (*s == '.' || *s == 'e' || *s == 'E')
                    {
                        isInteger = 0;
                        break;
                    }
                    s++;
                }

                if (isInteger)
                {
                    long val = atol(num->number);
                    if (val >= (-2147483647L - 1) && val <= 2147483647L)
                    {
                        res.type = DATA_TYPE_INTEGER;
                        res.value.iVal = val;
                    }
                    else
                    {
                        res.type = DATA_TYPE_DOUBLE;
                        res.value.dVal = atof(num->number);
                    }
                }
                else
                {
                    res.type = DATA_TYPE_DOUBLE;
                    res.value.dVal = atof(num->number);
                }
            }
            else
            {
                res.type = DATA_TYPE_DOUBLE;
                res.value.dVal = 0.0;
            }
            break;
        }
        case json_type_true:
            res.type = DATA_TYPE_LOGICAL;
            res.value.lVal = 1;
            break;

        case json_type_false:
            res.type = DATA_TYPE_LOGICAL;
            res.value.lVal = 0;
            break;

        case json_type_object:
        case json_type_array:
        {
            size_t bufSize = 65536;
            char *tempBuf = (char *)malloc(bufSize);
            res.type = DATA_TYPE_CHARACTER;
            if (tempBuf)
            {
                size_t written = Json_SerializeNode(r.node, tempBuf, bufSize - 1);
                tempBuf[written] = '\0';
                res.value.cVal = tempBuf;
            }
            else
            {
                res.value.cVal = _strdup("");
            }
            break;
        }
        default:
            res.type = DATA_TYPE_CHARACTER;
            res.value.cVal = _strdup("");
            break;
    }

    Json_Cleanup(&r);
    return res;
}

void JsonVariantFree(DataVariant *v)
{
    if (!v) return;
    if (v->type == DATA_TYPE_CHARACTER && v->value.cVal)
    {
        free(v->value.cVal);
        v->value.cVal = NULL;
    }
}
