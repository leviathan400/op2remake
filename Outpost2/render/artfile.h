#pragma once
#include <QImage>
#include <QString>
#include <cstdint>
#include <vector>

// =============================================================================
// ArtFile  —  loads OP2 sprite art (op2_art.prt + OP2_ART.BMP)
// =============================================================================
//
// op2_art.prt holds: palettes (CPAL), image definitions (offset into the BMP
// pixel atlas + dims + type + palette), and a group -> frame -> picture
// animation hierarchy. OP2_ART.BMP is the 8-bit pixel atlas; each image's
// dataPtr is an offset into it. This loader parses the .prt, keeps the BMP
// pixel blob, and renders any image to a QImage (palette index 0 -> transparent).
//
// Render-layer (uses QImage); the sim deals in unit/animation indices, not pixels.

class ArtFile {
public:
    struct ImageDef {
        std::int32_t pitch = 0;     // ScanLineByteWidth
        std::int32_t dataPtr = 0;   // byte offset into the BMP pixel blob
        std::int32_t height = 0;
        std::int32_t width = 0;
        std::int16_t type = 0;      // 0 menu, 1 in-game, 4/5 shadow (1bpp), ...
        std::int16_t paletteNum = 0;
    };

    // A positioned image within a frame.
    struct Picture {
        std::int16_t imageIndex = 0;
        std::int16_t xOffset = 0;
        std::int16_t yOffset = 0;
        std::uint8_t order = 0;   // draw order within the frame (low under, high on top)
    };

    // A per-group "appendix" — 4 ints. These carry the light points (the green
    // dots in OP2ArtViewer); exact field meaning decoded in groupAppendix dump.
    struct Appendix {
        std::int32_t a = 0, b = 0, c = 0, d = 0;
    };

    bool load(const QString& prtPath, const QString& bmpPath, QString* error = nullptr);

    int paletteCount() const { return static_cast<int>(palettes_.size()); }
    int imageCount() const { return static_cast<int>(images_.size()); }
    int groupCount() const { return static_cast<int>(groups_.size()); }
    int frameCount() const { return static_cast<int>(frames_.size()); }
    int pictureCount() const { return static_cast<int>(pictures_.size()); }

    const ImageDef* image(int index) const;

    // Render image[index] to an ARGB32 QImage (palette index 0 = transparent).
    QImage imageToQImage(int index) const;

    // Composite a frame (its positioned pictures) into one ARGB32 QImage.
    // anchorX/Y receive where the frame's logical origin (0,0) lands inside the
    // returned image. Shadow pictures (image types 4/5, 1bpp) are skipped by
    // default — they belong drawn under the sprite onto terrain, not composited
    // into the sprite (TODO: real drop shadows).
    QImage frameToQImage(int frameIndex, int* anchorX = nullptr,
                         int* anchorY = nullptr, bool includeShadows = false) const;

    // --- animation navigation (for compositing unit frames later) ---------
    int groupFrameStart(int group) const;  // first frame index of a group, -1
    int groupFrameCount(int group) const;
    int groupOfFrame(int frameIndex) const; // group owning an absolute frame, -1 if none
    int framePictureStart(int frame) const;
    int framePictureCount(int frame) const;
    int frameDelay(int frame) const; // game ticks to hold this frame (art-defined)
    const Picture* picture(int index) const;

    // The frame's optional turret-mount offset (x,y) — what OP2 adds to the unit
    // pixel to draw a combat chassis's turret on the hull for this rotation/frame.
    // Returns false (and x=y=0) if the frame has no optional block.
    bool frameTurretOffset(int frame, int* x, int* y) const;

    int groupAppendixStart(int group) const;
    int groupAppendixCount(int group) const;
    const Appendix* appendix(int index) const;

    // Group's pixel-displacement anchor + selection box (relative to the frame's
    // logical origin). OP2 draws pictures at unitPixel + displacement + offset.
    void groupDisplacement(int group, int* x, int* y) const;
    void groupSelectionBox(int group, int* l, int* t, int* r, int* b) const;

private:
    struct Palette { std::uint32_t colors[256]; }; // QRgb (0xAARRGGBB), opaque
    struct Frame {
        std::uint8_t numPictures = 0;
        std::uint8_t delay = 0;
        int startPicture = 0;
        // Optional per-frame block (frame flag bit 0x80): the (x,y) the engine adds
        // to the unit pixel when drawing the TURRET on a combat chassis frame —
        // i.e. the per-rotation turret mount point (OP2 Graphics.GetFrameOptionalInfo,
        // used in Unit:Tank.DrawUnit). signed bytes.
        std::int8_t optX = 0, optY = 0;
        bool hasOpt = false;
    };
    struct Group {
        std::int32_t numFrames = 0;
        int startFrame = 0;
        int startAppendix = 0;
        int numAppendices = 0;
        std::int32_t dispX = 0, dispY = 0;            // sprite anchor displacement
        std::int32_t selL = 0, selT = 0, selR = 0, selB = 0; // selection box
    };

    std::vector<Palette> palettes_;
    std::vector<ImageDef> images_;
    std::vector<Group> groups_;
    std::vector<Frame> frames_;
    std::vector<Picture> pictures_;
    std::vector<Appendix> appendices_;
    std::vector<std::uint8_t> pixels_; // OP2_ART.BMP pixel blob
};
