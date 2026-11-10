#ifndef CORE_JSON_H
#define CORE_JSON_H
/*
   The latest version of this library is available on GitHub;
   https://github.com/sheredom/json.h.
*/
// ============================================================================
// core_json.h
// Wrapper de alto nivel sobre sheredom/json.h con navegación por path tipo
// "results[0].shipping.id". NO depende del SDK de VFP: reusable en cualquier
// proyecto C. vfp_json.c consume esta librería y solo agrega la traducción
// hacia/desde FoxPro (ParamBlk, SafeRetValString, etc.).
//
// CONTRATO DE OWNERSHIP (importante):
//   - Las funciones que devuelven char* (no const) alocan memoria nueva.
//     El caller SIEMPRE debe liberarla con free().
//   - Las que devuelven const char* devuelven literales estáticos
//     ("string", "missing", etc.). NUNCA hay que liberarlos.
//   - JsonGetAny devuelve un DataVariant; si .type == DATA_TYPE_CHARACTER,
//     .value.cVal debe liberarse con JsonVariantFree(), no con free() directo
//     (por si en el futuro se agrega lógica adicional de limpieza).
// ============================================================================

#include "engine/json.h"

#ifdef __cplusplus
extern "C" {
#endif


#include "core.h"
//#include "../../encoding/core/core_encoding.h"
#include "core_string.h"
    
// ============================================================================
// TIPOS
// ============================================================================

typedef enum {
    DATA_TYPE_EMPTY,
    DATA_TYPE_CHARACTER,
    DATA_TYPE_INTEGER,
    DATA_TYPE_DOUBLE,
    DATA_TYPE_LOGICAL
} DataType;

typedef struct {
    DataType type;
    union {
        char *cVal;   // DATA_TYPE_CHARACTER: liberar con JsonVariantFree()
        long  iVal;   // DATA_TYPE_INTEGER
        double dVal;  // DATA_TYPE_DOUBLE
        int   lVal;   // DATA_TYPE_LOGICAL (0/1)
    } value;
} DataVariant;

// ============================================================================
// API PÚBLICA
// ----------------------------------------------------------------------------
// json: buffer del documento JSON completo (null-terminated).
// path: expresión de navegación tipo "results[0].shipping.id".
// ============================================================================

// Devuelve una COPIA del string en el path. Liberar con free().
// Devuelve "" (copia de string vacío, también liberable) si no se encontró
// o el nodo no es de tipo string.
CORE_API char *JsonGetString(const char *json, const char *path);

CORE_API double JsonGetDouble(const char *json, const char *path);

// Devuelve el entero de 64 bits como string (para IDs que exceden precisión
// de double). Buffer alocado en heap. Liberar con free().
CORE_API char *JsonGetInt64(const char *json, const char *path);

CORE_API long JsonGetInt(const char *json, const char *path);

CORE_API int JsonGetBool(const char *json, const char *path);

// 1 si el path existe y no es JSON null; 0 en cualquier otro caso.
CORE_API int JsonHasValue(const char *json, const char *path);

CORE_API int JsonArrayCount(const char *json, const char *path);

// Devuelve un literal ESTÁTICO ("string"|"number"|"object"|"array"|"bool"|
// "null"|"missing"|"unknown"). NUNCA liberar el resultado.
CORE_API const char *JsonGetType(const char *json, const char *path);

// Serializa el sub-árbol en el path como texto JSON. Copia nueva en heap.
// Liberar con free(). Trunca en 64KB (ver nota en la implementación).
CORE_API char *JsonExtractSubNode(const char *json, const char *path);

// Detecta el tipo del valor y lo devuelve en el tipo VFP-nativo más
// apropiado. Si res.type == DATA_TYPE_CHARACTER, liberar res.value.cVal
// con JsonVariantFree(&res) -- NO con free() directo.
CORE_API DataVariant JsonGetAny(const char *json, const char *path);

// Libera la memoria interna de un DataVariant devuelto por JsonGetAny.
// Es seguro llamarla siempre, incluso si type no es DATA_TYPE_CHARACTER
// (en ese caso no hace nada).
CORE_API void JsonVariantFree(DataVariant *v);

//char *CleanSpacesJson(const char *json); 

#ifdef __cplusplus
}
#endif

#endif // CORE_JSON_H
