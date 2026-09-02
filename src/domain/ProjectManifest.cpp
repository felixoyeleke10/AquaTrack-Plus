#include "domain/ProjectManifest.h"

namespace aquatrack::domain {

bool isValid(const ProjectManifest& manifest, std::string* reason)
{
    const auto fail = [reason](const char* message) {
        if (reason)
            *reason = message;
        return false;
    };
    if (manifest.schemaVersion != CurrentProjectSchemaVersion)
        return fail("unsupported project schema version");
    if (manifest.projectId.empty())
        return fail("project id is required");
    if (manifest.name.empty())
        return fail("project name is required");
    if (manifest.createdAtMs < 0 || manifest.updatedAtMs < manifest.createdAtMs)
        return fail("project timestamps are invalid");
    if (manifest.databasePath.empty())
        return fail("database path is required");
    return true;
}

}  // namespace aquatrack::domain
