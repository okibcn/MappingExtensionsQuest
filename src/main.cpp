/*

███╗   ███╗ █████╗ ██████╗ ██████╗ ██╗███╗   ██╗ ██████╗
████╗ ████║██╔══██╗██╔══██╗██╔══██╗██║████╗  ██║██╔════╝
██╔████╔██║███████║██████╔╝██████╔╝██║██╔██╗ ██║██║  ███╗
██║╚██╔╝██║██╔══██║██╔═══╝ ██╔═══╝ ██║██║╚██╗██║██║   ██║
██║ ╚═╝ ██║██║  ██║██║     ██║     ██║██║ ╚████║╚██████╔╝
╚═╝     ╚═╝╚═╝  ╚═╝╚═╝     ╚═╝     ╚═╝╚═╝  ╚═══╝ ╚═════╝

███████╗██╗  ██╗████████╗███████╗███╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗███████╗
██╔════╝╚██╗██╔╝╚══██╔══╝██╔════╝████╗  ██║██╔════╝██║██╔═══██╗████╗  ██║██╔════╝
█████╗   ╚███╔╝    ██║   █████╗  ██╔██╗ ██║███████╗██║██║   ██║██╔██╗ ██║███████╗
██╔══╝   ██╔██╗    ██║   ██╔══╝  ██║╚██╗██║╚════██║██║██║   ██║██║╚██╗██║╚════██║
███████╗██╔╝ ██╗   ██║   ███████╗██║ ╚████║███████║██║╚██████╔╝██║ ╚████║███████║
╚══════╝╚═╝  ╚═╝   ╚═╝   ╚══════╝╚═╝  ╚═══╝╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝╚══════╝

Apocrypha version of the official version by Mioki

*/

#include "modloader/shared/modloader.hpp"

#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/BeatmapDataObstaclesMergingTransform.hpp"
#include "GlobalNamespace/BeatmapObjectExecutionRatingsRecorder.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnController_InitData.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnMovementData.hpp"
#include "GlobalNamespace/FlyingScoreSpawner.hpp"
#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/IDifficultyBeatmapSet.hpp"
#include "GlobalNamespace/MainMenuViewController.hpp"
#include "GlobalNamespace/NoteCutDirectionExtensions.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/ObstacleController.hpp"
#include "GlobalNamespace/ObstacleData.hpp"
#include "GlobalNamespace/SpawnRotationProcessor.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/StretchableObstacle.hpp"

#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"

#include "pinkcore/shared/RequirementAPI.hpp"

using namespace GlobalNamespace;

ModInfo modInfo;
Logger& logger()
{
    static auto logger = new Logger(modInfo, LoggerOptions(false, true));
    return *logger;
}

extern "C" void setup(ModInfo& info)
{
    info.id      = "MappingExtensions";
    info.version = VERSION;
    modInfo      = info;
    logger().info("Leaving setup!");
}

// Normalized indices are faster to compute & reverse, and more accurate than, effective indices (see below).
// A "normalized" precision index is an effective index * 1000. So unlike normal precision indices, only 0 is 0.
int ToNormalizedPrecisionIndex(int index)
{
    if (index <= -1000) {
        return index + 1000;
    } else {
        return ((index >= 1000) ? (index - 1000) : index * 1000);
    }
}
int FromNormalizedPrecisionIndex(int index)
{
    if (index % 1000 == 0) {
        return index / 1000;
    } else {
        return ((index > 0) ? (index + 1000) : (index - 1000));
    }
}

// An effective index is a normal/extended index, but with decimal places that do what you'd expect.
float ToEffectiveIndex(int index)
{
    if (index <= -1000) {
        return index / 1000.0f + 1.0f;
    } else {
        return ((index >= 1000) ? (index / 1000.0f - 1.0f) : index);
    }
}

static BeatmapCharacteristicSO* storedBeatmapCharacteristicSO = nullptr;

MAKE_HOOK_MATCH(
    StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self)
{
    StandardLevelDetailView_RefreshContent(self);
    storedBeatmapCharacteristicSO = self->selectedDifficultyBeatmap->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic();
}

MAKE_HOOK_MATCH(MainMenuViewController_DidActivate, &MainMenuViewController::DidActivate, void, MainMenuViewController* self,
    bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling)
{
    storedBeatmapCharacteristicSO = nullptr;
    return MainMenuViewController_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);
}

static bool skipWallRatings = false;
MAKE_HOOK_MATCH(BeatmapObjectSpawnController_Start, &BeatmapObjectSpawnController::Start, void, BeatmapObjectSpawnController* self)
{
    skipWallRatings = false;
    return BeatmapObjectSpawnController_Start(self);
}

MAKE_HOOK_MATCH(BeatmapObjectExecutionRatingsRecorder_HandleObstacleDidPassAvoidedMark,
    &BeatmapObjectExecutionRatingsRecorder::HandleObstacleDidPassAvoidedMark, void, BeatmapObjectExecutionRatingsRecorder* self,
    ObstacleController* obstacleController)
{
    if (skipWallRatings) {
        return;
    } else {
        return BeatmapObjectExecutionRatingsRecorder_HandleObstacleDidPassAvoidedMark(self, obstacleController);
    }
}

/* PC version hooks */

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_GetNoteOffset, &BeatmapObjectSpawnMovementData::GetNoteOffset, UnityEngine::Vector3,
    BeatmapObjectSpawnMovementData* self, int noteLineIndex, GlobalNamespace::NoteLineLayer noteLineLayer)
{
    if (noteLineIndex == 4839) {
        logger().info("lineIndex %i and lineLayer %i!", noteLineIndex, noteLineLayer.value);
    }
    auto __result = BeatmapObjectSpawnMovementData_GetNoteOffset(self, noteLineIndex, noteLineLayer);
    // if (!Plugin.active) return __result;

    if (noteLineIndex >= 1000 || noteLineIndex <= -1000) {
        if (noteLineIndex <= -1000)
            noteLineIndex += 2000;
        float xPos = -(self->noteLinesCount - 1.0f) * 0.5f;
        xPos += ((float)noteLineIndex * self->noteLinesDistance / 1000.0f);

        float yPos = self->LineYPosForLineLayer(noteLineLayer);
        __result   = UnityEngine::Vector3(xPos, yPos, 0.0f);
    }
    return __result;
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer,
    &BeatmapObjectSpawnMovementData::HighestJumpPosYForLineLayer, float, BeatmapObjectSpawnMovementData* self,
    NoteLineLayer lineLayer)
{
    float __result = BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer(self, lineLayer);
    float delta = (self->topLinesHighestJumpPosY - self->upperLinesHighestJumpPosY);

    if (lineLayer >= 1000 || lineLayer <= -1000) {
        __result = self->upperLinesHighestJumpPosY - delta - delta + self->jumpOffsetY + (lineLayer * (delta / 1000.0f));
    } else if (lineLayer > 2 || lineLayer < 0) {
        __result = self->upperLinesHighestJumpPosY - delta + self->jumpOffsetY + (lineLayer * delta);
    }
    if (__result > 2.9f) {
        logger().warning("Extreme note jump! lineLayer %i gave jump %f!", (int)lineLayer, __result);
    }
    return __result;
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_LineYPosForLineLayer, &BeatmapObjectSpawnMovementData::LineYPosForLineLayer, float,
    BeatmapObjectSpawnMovementData* self, NoteLineLayer lineLayer)
{
    float __result = BeatmapObjectSpawnMovementData_LineYPosForLineLayer(self, lineLayer);
    float delta = (self->topLinesYPos - self->upperLinesYPos);

    if (lineLayer >= 1000 || lineLayer <= -1000) {
        __result = self->upperLinesYPos - delta - delta + (lineLayer * delta / 1000.0f);
    } else if (lineLayer > 2 || lineLayer < 0) {
        __result = self->upperLinesYPos - delta + (lineLayer * delta);
    }
    if (__result > 1.9f) {
        logger().warning("Extreme note position! lineLayer %i gave YPos %f!", (int)lineLayer, __result);
    }
    return __result;
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_Get2DNoteOffset, &BeatmapObjectSpawnMovementData::Get2DNoteOffset,
    UnityEngine::Vector2, BeatmapObjectSpawnMovementData* self, int noteLineIndex, GlobalNamespace::NoteLineLayer noteLineLayer)
{
    if (noteLineIndex == 4839) {
        logger().info("lineIndex %i and lineLayer %i!", noteLineIndex, noteLineLayer.value);
    }
    auto __result = BeatmapObjectSpawnMovementData_Get2DNoteOffset(self, noteLineIndex, noteLineLayer);
    if (noteLineIndex >= 1000 || noteLineIndex <= -1000) {
        if (noteLineIndex <= -1000)
            noteLineIndex += 2000;
        float x   = -(self->noteLinesCount - 1.0f) * 0.5f;
        x        += ((float)noteLineIndex * self->noteLinesDistance / 1000.0f);
        float y   = self->LineYPosForLineLayer(noteLineLayer);
        __result  = UnityEngine::Vector2(x, y);
    }
    return __result;
}

MAKE_HOOK_MATCH(FlyingScoreSpawner_SpawnFlyingScore, &FlyingScoreSpawner::SpawnFlyingScore, void, FlyingScoreSpawner* self,
    ByRef<GlobalNamespace::NoteCutInfo> noteCutInfo, int noteLineIndex, int multiplier, UnityEngine::Vector3 pos,
    UnityEngine::Quaternion rotation, UnityEngine::Quaternion inverseRotation, UnityEngine::Color color)
{
    if (noteLineIndex < 0)
        noteLineIndex = 0;
    if (noteLineIndex > 3)
        noteLineIndex = 3;
    return FlyingScoreSpawner_SpawnFlyingScore(
        self, noteCutInfo, noteLineIndex, multiplier, pos, rotation, inverseRotation, color);
}

MAKE_HOOK_MATCH(
    NoteCutDirectionExtensions_RotationAngle, &NoteCutDirectionExtensions::RotationAngle, float, NoteCutDirection cutDirection)
{
    float __result = NoteCutDirectionExtensions_RotationAngle(cutDirection);
    if (cutDirection >= 1000 && cutDirection <= 1360) {
        __result = 1000 - cutDirection;
    } else if (cutDirection >= 2000 && cutDirection <= 2360) {
        __result = 2000 - cutDirection;
    }
    return __result;
}
MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Direction, &NoteCutDirectionExtensions::Direction, UnityEngine::Vector2,
    NoteCutDirection cutDirection)
{
    UnityEngine::Vector2 __result = NoteCutDirectionExtensions_Direction(cutDirection);
    if ((cutDirection >= 1000 && cutDirection <= 1360) || (cutDirection >= 2000 && cutDirection <= 2360)) {
        // uses RotationAngle hook indirectly
        auto quaternion          = NoteCutDirectionExtensions::Rotation(cutDirection, 0.0f);
        static auto forward      = UnityEngine::Vector3::get_forward();
        UnityEngine::Vector3 dir = quaternion * forward;
        __result                 = UnityEngine::Vector2(dir.x, dir.y);
        // logger().debug("NoteCutDirectionExtensions: {%f, %f}", dir.x, dir.y);
    }
    return __result;
}

bool MirrorPrecisionLineIndex(int& lineIndex, int lineCount)
{
    if (lineIndex >= 1000 || lineIndex <= -1000) {
        bool notVanillaRange = (lineIndex <= 0 || lineIndex > lineCount * 1000);

        int newIndex = (lineCount + 1) * 1000 - lineIndex;
        if (notVanillaRange)
            newIndex -= 2000; // this fixes the skip between 1000 and -1000 which happens once iff start or end is negative
        lineIndex = newIndex;
        return true;
    }
    return false;
}

MAKE_HOOK_MATCH(ObstacleController_Init, &ObstacleController::Init, void, ObstacleController* self, ObstacleData* obstacleData,
    float worldRotation, UnityEngine::Vector3 startPos, UnityEngine::Vector3 midPos, UnityEngine::Vector3 endPos,
    float move1Duration, float move2Duration, float singleLineWidth, float height)
{
    ObstacleController_Init(
        self, obstacleData, worldRotation, startPos, midPos, endPos, move1Duration, move2Duration, singleLineWidth, height);

    int width = obstacleData->width;
    int value = obstacleData->obstacleType.value;
    if ((value < 1000) && (width < 1000))
        return;
    
    // From here on either obstacle height, start, or width are precision at this point
    skipWallRatings = true;
    bool preciseWidth = (width >= 1000);
    bool preciseHeight = (value >= 1000);
    bool preciseHeightStart = (value >= 4001 && value <= 4100000);

    // unpacks obstacle width 
    float obsWidth = (preciseWidth) ? ((float)(width - 1000) / 1000.0f)  : 
                                      ((float)width);
    obsWidth *= singleLineWidth;

    // calculates obstacle lenght
    float obsLength  = UnityEngine::Vector3::Distance(self->endPos, self->midPos) * (obstacleData->duration / move2Duration);
    
    //Unpacks obstacle Start and move the start position vector
    int startHeight = (preciseHeightStart) ? ((value-4001) % 1000) : 0;
    if (preciseWidth || preciseHeightStart) {
        // Change y of b for start height
        UnityEngine::Vector3 b { b.x = (obsWidth - singleLineWidth) * 0.5f, b.y = (float)(startHeight << 2) / 1000.0f, b.z = 0.0f };
        self->startPos = startPos + b;
        self->midPos   = midPos + b;
        self->endPos   = endPos + b;
    }

    //Unpacks obstacle Height Multiplier
    int obsHeight = (preciseHeightStart) ? ((value-4001) / 1000)                  // Precise Height and Start mode
                                         : value - 1000;
    float heightMultiplier = (preciseHeight) ? (float)obsHeight / 1000 : 1.0f;

    obsWidth *= 0.98f;
    self->stretchableObstacle->SetSizeAndColor(obsWidth, (height * heightMultiplier), obsLength, self->color);
    self->bounds = self->stretchableObstacle->bounds;
}



MAKE_HOOK_MATCH(NoteCutDirection_Mirror, &NoteData::Mirror, void, NoteData* self, int lineCount)
{
    logger().debug("Mirroring note with time: %f, lineIndex: %i, lineLayer: %i, cutDirection: %i", self->time, self->lineIndex,
        (int)(self->noteLineLayer), (int)(self->cutDirection));

    int lineIndex     = self->lineIndex;
    int flipLineIndex = self->flipLineIndex;
    NoteCutDirection_Mirror(self, lineCount);
    if (MirrorPrecisionLineIndex(lineIndex, lineCount))
        self->lineIndex = lineIndex;
    if (MirrorPrecisionLineIndex(flipLineIndex, lineCount))
        self->flipLineIndex = flipLineIndex;
}

// MAKE_HOOK_MATCH(NoteData_MirrorTransformCutDirection, &NoteData::cutDirection, void, NoteData* self)
MAKE_HOOK_MATCH(NoteCutDirection_MirrorTransformCutDirection, &NoteData::Mirror, void, NoteData* self, int linecount)
{
    int state = self->cutDirection.value;
    NoteCutDirection_MirrorTransformCutDirection(self, linecount);
    if (state >= 1000) {
        int newdir         = 2360 - state;
        self->cutDirection = newdir;
    }
}

MAKE_HOOK_MATCH(ObstacleData_Mirror, &ObstacleData::Mirror, void, ObstacleData* self, int lineCount)
{
    int __state         = self->lineIndex;
    bool precisionWidth = (self->width >= 1000);
    ObstacleData_Mirror(self, lineCount);

    logger().debug("lineCount: %i", lineCount);
    if (__state >= 1000 || __state <= -1000 || precisionWidth) {
        int normIndex = ToNormalizedPrecisionIndex(__state);
        int normWidth = ToNormalizedPrecisionIndex(self->width);

        // The vanilla formula * 1000
        int normNewIndex = lineCount * 1000 - normWidth - normIndex;

        self->lineIndex = FromNormalizedPrecisionIndex(normNewIndex);
        logger().debug("wall (of type %i) with lineIndex %i (norm %i) and width %i (norm %i) mirrored to %i (norm %i)",
            (int)(self->obstacleType), __state, normIndex, self->width, normWidth, self->lineIndex, normNewIndex);
    }
}

MAKE_HOOK_MATCH(SpawnRotationProcessor_RotationForEventValue, &SpawnRotationProcessor::RotationForEventValue, float,
    SpawnRotationProcessor* self, int index)
{
    float __result = SpawnRotationProcessor_RotationForEventValue(self, index);
    return (storedBeatmapCharacteristicSO->requires360Movement && (index >= 1000)) ? index - 1360.0f : __result;
}

/* End of PC version hooks */

MAKE_HOOK_MATCH(BeatmapDataObstaclesMergingTransform_CreateTransformedData,
    &BeatmapDataObstaclesMergingTransform::CreateTransformedData, IReadonlyBeatmapData*, IReadonlyBeatmapData* beatmapData)
{
    return beatmapData;
}

extern "C" void load()
{
    logger().info("Installing MappingExtensions Hooks!");
    il2cpp_functions::Init();

    Logger& hookLogger = logger();

    // hooks to help us get the selected beatmap info
    INSTALL_HOOK(hookLogger, StandardLevelDetailView_RefreshContent);
    INSTALL_HOOK(hookLogger, MainMenuViewController_DidActivate);

    // PC version hooks
    INSTALL_HOOK(hookLogger, BeatmapObjectSpawnMovementData_GetNoteOffset);
    INSTALL_HOOK(hookLogger, BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer);
    INSTALL_HOOK(hookLogger, BeatmapObjectSpawnMovementData_LineYPosForLineLayer);
    INSTALL_HOOK(hookLogger, BeatmapObjectSpawnMovementData_Get2DNoteOffset);
    INSTALL_HOOK(hookLogger, FlyingScoreSpawner_SpawnFlyingScore);
    INSTALL_HOOK(hookLogger, NoteCutDirectionExtensions_RotationAngle);
    INSTALL_HOOK(hookLogger, NoteCutDirectionExtensions_Direction);
    INSTALL_HOOK(hookLogger, NoteCutDirection_Mirror);
    INSTALL_HOOK(hookLogger, NoteCutDirection_MirrorTransformCutDirection);
    INSTALL_HOOK(hookLogger, ObstacleController_Init);
    INSTALL_HOOK(hookLogger, ObstacleData_Mirror);
    INSTALL_HOOK(hookLogger, SpawnRotationProcessor_RotationForEventValue);

    // Avoids wall penalty for artistic obstacles
    INSTALL_HOOK(hookLogger, BeatmapObjectSpawnController_Start);
    INSTALL_HOOK(hookLogger, BeatmapObjectExecutionRatingsRecorder_HandleObstacleDidPassAvoidedMark);

    // Skip pepega merging
    INSTALL_HOOK(hookLogger, BeatmapDataObstaclesMergingTransform_CreateTransformedData);


    logger().info("Installed MappingExtensions Hooks!");

    // Register Requirement for playing ME maps
    PinkCore::RequirementAPI::RegisterInstalled("Mapping Extensions");
    // PinkCore::RequirementAPI::RegisterInstalled("Noodle Extensions");  // DON'T EVEN THINK ABOUT IT!!!!
}
