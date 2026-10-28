/* SPDX-License-Identifier: MIT */
#include "canvas.h"

#include "geometry.h"
#include "motion_model.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace kiwi::studio {
namespace {

using namespace pet::shape;

Vec2 cubic(Vec2 a, Vec2 b, Vec2 c, Vec2 d, float t)
{
    const float u = 1 - t;
    return {u * u * u * a.x + 3 * u * u * t * b.x + 3 * u * t * t * c.x + t * t * t * d.x,
            u * u * u * a.y + 3 * u * u * t * b.y + 3 * u * t * t * c.y + t * t * t * d.y};
}

ImU32 rgba(Color c, float a = 1)
{
    return IM_COL32(c.red, c.green, c.blue, int(std::clamp(a, 0.f, 1.f) * 255));
}

std::vector<ImVec2> flatten(const ShapePath& p, ImVec2 o, float z)
{
    std::vector<ImVec2> out;
    Vec2 cur{};
    for (uint8_t i = 0; i < p.command_count; i++) {
        auto& q = p.commands[i];
        if (q.type == PathCommandType::Move) {
            cur = q.first;
            out.push_back({o.x + cur.x * z, o.y + cur.y * z});
        } else if (q.type == PathCommandType::Cubic) {
            for (int j = 1; j <= 16; j++) {
                cur = cubic(cur, q.first, q.second, q.end, float(j) / 16);
                out.push_back({o.x + cur.x * z, o.y + cur.y * z});
            }
        }
    }
    return out;
}

}  // namespace

bool drawCanvas(SceneDoc& document, EditorState& editor)
{
    ImGui::BeginChild("canvas", {}, true);
    SceneDoc display = document;
    if (editor.motion.playing) {
        const uint32_t elapsed_ms = static_cast<uint32_t>(std::max(0.0f, editor.motion.time_seconds) * 1000.0f);
        for (size_t index = 0; index < display.count; ++index) {
            MotionPreview preview = display.nodes[index].motion;
            preview.playing       = true;
            preview.time_seconds  = editor.motion.time_seconds;
            if (preview.enabled) applyMotion(display.nodes[index], preview, elapsed_ms);
        }
    }
    auto a          = ImGui::GetContentRegionAvail();
    const float fit = std::max(.1f, std::min((a.x - 10) / kDisplayWidth, (a.y - 10) / kDisplayHeight));
    bool changed    = false;
    std::array<Affine2D, kMaxNodes> worlds{};
    const bool hierarchy_valid = resolveWorlds(display, worlds);

    if (editor.reset_requested) {
        editor.reset_requested = false;
        editor.zoom            = 1.0f;
        editor.pan             = {};
    }
    if (editor.frame_requested) {
        editor.frame_requested = false;
        Bounds2D content{};
        if (hierarchy_valid)
            for (size_t index = 0; index < document.count; ++index) {
                if (!display.nodes[index].visible) continue;
                ShapePath path;
                if (!sampleNode(display.nodes[index], worlds[index], path) || !path.bounds.valid) continue;
                include(content, path.bounds.minimum);
                include(content, path.bounds.maximum);
            }
        if (content.valid) {
            const float width  = std::max(1.0f, content.maximum.x - content.minimum.x);
            const float height = std::max(1.0f, content.maximum.y - content.minimum.y);
            editor.zoom        = std::clamp(std::min(kDisplayWidth / width, kDisplayHeight / height) * .9f, .25f, 6.0f);
            const float z      = fit * editor.zoom;
            editor.pan.x       = a.x * .5f - (content.minimum.x + content.maximum.x) * .5f * z;
            editor.pan.y       = a.y * .5f - (content.minimum.y + content.maximum.y) * .5f * z;
        } else {
            editor.zoom = 1.0f;
            editor.pan  = {};
        }
    }
    if (ImGui::IsWindowHovered()) {
        if (ImGui::GetIO().MouseWheel != 0.0f)
            editor.zoom = std::clamp(editor.zoom * std::pow(1.15f, ImGui::GetIO().MouseWheel), 0.25f, 6.0f);
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            editor.pan.x += ImGui::GetIO().MouseDelta.x;
            editor.pan.y += ImGui::GetIO().MouseDelta.y;
        }
    }
    const float z       = fit * editor.zoom;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 o{origin.x + editor.pan.x, origin.y + editor.pan.y};
    auto* d = ImGui::GetWindowDrawList();
    d->AddRectFilled(o, {o.x + kDisplayWidth * z, o.y + kDisplayHeight * z}, rgba(document.background));
    if (editor.show_guides) {
        const ImU32 guide = IM_COL32(90, 120, 140, 160);
        d->AddLine({o.x + kDisplayWidth * .5f * z, o.y}, {o.x + kDisplayWidth * .5f * z, o.y + kDisplayHeight * z},
                   guide);
        d->AddLine({o.x, o.y + kDisplayHeight * .5f * z}, {o.x + kDisplayWidth * z, o.y + kDisplayHeight * .5f * z},
                   guide);
    }
    d->AddRect(o, {o.x + kDisplayWidth * z, o.y + kDisplayHeight * z}, IM_COL32(110, 130, 145, 255));
    if (!hierarchy_valid) {
        d->AddText({o.x + 4, o.y + 4}, IM_COL32(255, 120, 120, 255), "Invalid parent ordering");
    }

    // --- Control-handle geometry -------------------------------------------------
    // Built once per frame so drawing and hit-testing share identical screen
    // positions. Handles are derived from the polygon's local cubic geometry and
    // pushed through the node's world transform.
    const auto vsub  = [](Vec2 left, Vec2 right) { return Vec2{left.x - right.x, left.y - right.y}; };
    const auto vdot  = [](Vec2 left, Vec2 right) { return left.x * right.x + left.y * right.y; };
    const auto vlen  = [](Vec2 value) { return std::sqrt(value.x * value.x + value.y * value.y); };
    const auto vnorm = [&](Vec2 value) {
        const float len = vlen(value);
        return len > 1e-6f ? Vec2{value.x / len, value.y / len} : Vec2{};
    };
    const auto vadd   = [](Vec2 left, Vec2 right) { return Vec2{left.x + right.x, left.y + right.y}; };
    const auto vscale = [](Vec2 value, float factor) { return Vec2{value.x * factor, value.y * factor}; };
    const auto vcross = [](Vec2 left, Vec2 right) { return left.x * right.y - left.y * right.x; };
    struct CanvasHandle {
        // 0=edge in, 1=edge out, 2=corner tip A, 3=corner tip B, 4=vertex position.
        int kind  = 0;
        int index = 0;
        ImVec2 point{};
        ImVec2 anchor{};
    };
    constexpr size_t kMaxCanvasHandles = kMaxRoundedPolygonVertices * 5;
    std::array<CanvasHandle, kMaxCanvasHandles> canvas_handles{};
    size_t canvas_handle_count = 0;
    bool handle_edit_ready     = false;
    Affine2D handle_world_inverse{};
    RoundedPolygonSample handle_sample{};
    int hovered_handle = -1;
    // Fixed buffer: this runs every frame, so avoid per-frame heap allocation.
    char handle_tip[192]     = {};
    const auto handle_colour = [](int kind) {
        switch (kind) {
            case 4:
                return IM_COL32(130, 190, 255, 255);  // vertex
            case 2:
            case 3:
                return IM_COL32(120, 215, 255, 255);  // corner tips
            default:
                return IM_COL32(255, 140, 210, 255);  // edge handles
        }
    };
    const auto handle_label = [](int kind) {
        switch (kind) {
            case 0:
                return "edge in";
            case 1:
                return "edge out";
            case 2:
                return "corner tip A";
            case 3:
                return "corner tip B";
            default:
                return "vertex";
        }
    };
    // Reads authored parameters so tips describe the document being edited, not the
    // animated preview copy. Reused for hover and for the active drag.
    const auto describe_handle = [&](int kind, int index, char* out, size_t size) {
        if (editor.selection < 0 || size_t(editor.selection) >= document.count) {
            snprintf(out, size, "Handle");
            return;
        }
        const Node& node = document.nodes[editor.selection];
        if (kind == 4) {
            snprintf(out, size, "Vertex %d · drag to move", index);
        } else if (kind <= 1) {
            const auto& edge    = node.polygon.edges[index];
            const float trim    = kind == 0 ? edge.start : edge.end;
            const bool at_limit = std::fabs(edge.bulge) >= kMaxEdgeBulge - 1e-4f;
            snprintf(out, size, "Edge %d %s · bulge %.2f%s · %.2f", index, handle_label(kind), edge.bulge,
                     at_limit ? " (limit)" : "", trim);
        } else {
            const auto& corner  = node.polygon.corners[index];
            const bool at_limit = corner.roundness >= kMaxCornerRoundness - 1e-4f ||
                                  corner.cap_tension <= kMinCapTension + 1e-4f ||
                                  corner.cap_tension >= kMaxCapTension - 1e-4f;
            snprintf(out, size, "Corner %d %s · round %.2f · tension %.2f%s", index, handle_label(kind),
                     corner.roundness, corner.cap_tension, at_limit ? " (limit)" : "");
        }
    };

    const auto to_screen = [&](const Affine2D& world, Vec2 local) {
        const Vec2 transformed = transformPoint(world, local);
        return ImVec2{o.x + transformed.x * z, o.y + transformed.y * z};
    };

    // Handles describe authored geometry. While the motion preview clock runs the drawn
    // shape is animated, so editing it would not correspond to a single authored value.
    if (hierarchy_valid && !editor.motion.playing && (editor.show_bezier_controls || editor.show_vertices) &&
        editor.selection >= 0 && editor.selection < static_cast<int>(document.count)) {
        const Node& selected = document.nodes[editor.selection];
        if (selected.visible && selected.kind == Kind::Polygon &&
            sampleRoundedPolygon(selected.polygon, worlds[editor.selection], handle_sample, selected.style) &&
            invert(worlds[editor.selection], handle_world_inverse)) {
            handle_edit_ready          = true;
            const uint8_t vertex_count = handle_sample.parameters.vertex_count;
            // Vertex controls first: edge/corner handles take hit-test priority on ties.
            if (editor.show_vertices)
                for (uint8_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
                    const ImVec2 point =
                        to_screen(worlds[editor.selection], handle_sample.parameters.vertices[vertex_index]);
                    canvas_handles[canvas_handle_count++] = {4, vertex_index, point, point};
                }
            if (editor.show_bezier_controls) {
                for (uint8_t edge_index = 0; edge_index < vertex_count; ++edge_index) {
                    const auto& edge                      = handle_sample.edges[edge_index];
                    canvas_handles[canvas_handle_count++] = {0, edge_index,
                                                             to_screen(worlds[editor.selection], edge.first_control),
                                                             to_screen(worlds[editor.selection], edge.start)};
                    canvas_handles[canvas_handle_count++] = {1, edge_index,
                                                             to_screen(worlds[editor.selection], edge.second_control),
                                                             to_screen(worlds[editor.selection], edge.end)};
                }
                for (uint8_t corner_index = 0; corner_index < vertex_count; ++corner_index) {
                    const auto& corner                    = handle_sample.corners[corner_index];
                    canvas_handles[canvas_handle_count++] = {2, corner_index,
                                                             to_screen(worlds[editor.selection], corner.first_control),
                                                             to_screen(worlds[editor.selection], corner.start)};
                    canvas_handles[canvas_handle_count++] = {3, corner_index,
                                                             to_screen(worlds[editor.selection], corner.second_control),
                                                             to_screen(worlds[editor.selection], corner.end)};
                }
            }
            if (ImGui::IsWindowHovered()) {
                const ImVec2 mouse = ImGui::GetIO().MousePos;
                float best         = 12.0f;
                for (size_t h = 0; h < canvas_handle_count; ++h) {
                    const float dx       = mouse.x - canvas_handles[h].point.x;
                    const float dy       = mouse.y - canvas_handles[h].point.y;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    if (distance <= best) {
                        best           = distance;
                        hovered_handle = static_cast<int>(h);
                    }
                }
                if (hovered_handle >= 0) {
                    const CanvasHandle& hovered = canvas_handles[hovered_handle];
                    describe_handle(hovered.kind, hovered.index, handle_tip, sizeof(handle_tip));
                }
            }
        }
    }
    // -----------------------------------------------------------------------------

    for (size_t i = 0; hierarchy_valid && i < display.count; i++) {
        const Node& node = display.nodes[i];
        if (!node.visible) continue;
        ShapePath p;
        if (!sampleNode(node, worlds[i], p)) continue;
        auto v = flatten(p, o, z);
        if (v.size() > 2 && p.style.fill)
            d->AddConcavePolyFilled(v.data(), int(v.size()), rgba(p.style.color, p.style.opacity));
        if (v.size() > 1 && p.style.stroke)
            d->AddPolyline(v.data(), int(v.size()), rgba(p.style.color, p.style.opacity), ImDrawFlags_Closed,
                           p.style.stroke_width * z);
        if (editor.show_bounds)
            d->AddRect({o.x + p.bounds.minimum.x * z, o.y + p.bounds.minimum.y * z},
                       {o.x + p.bounds.maximum.x * z, o.y + p.bounds.maximum.y * z}, IM_COL32(90, 120, 140, 180));
        if (int(i) == editor.selection) {
            d->AddRect({o.x + p.bounds.minimum.x * z, o.y + p.bounds.minimum.y * z},
                       {o.x + p.bounds.maximum.x * z, o.y + p.bounds.maximum.y * z}, IM_COL32(132, 204, 22, 255));
            const Vec2 pivot = transformPoint(worlds[i], node.transform.pivot);
            d->AddCircle({o.x + pivot.x * z, o.y + pivot.y * z}, 3.0f, IM_COL32(255, 200, 60, 255));
            if (node.kind == Kind::Polygon && editor.show_vertices && !(handle_edit_ready && editor.show_vertices)) {
                // Vertex handles replace the static dots whenever they are interactive.
                RoundedPolygonSample landmarks;
                if (sampleRoundedPolygon(node.polygon, worlds[i], landmarks, node.style))
                    for (uint8_t v = 0; v < landmarks.parameters.vertex_count; ++v)
                        d->AddCircle({o.x + landmarks.landmarks[v].x * z, o.y + landmarks.landmarks[v].y * z}, 2.5f,
                                     IM_COL32(120, 200, 255, 255));
            }
        }
    }
    if (handle_edit_ready && canvas_handle_count > 0) {
        const ImU32 hover_colour  = IM_COL32(255, 224, 96, 255);
        const ImU32 active_colour = IM_COL32(255, 255, 255, 255);
        for (size_t h = 0; h < canvas_handle_count; ++h) {
            const CanvasHandle& handle = canvas_handles[h];
            const bool active = editor.drag_handle_node == editor.selection && editor.drag_handle_kind == handle.kind &&
                                editor.drag_handle_index == handle.index;
            const bool hovered = !active && hovered_handle == static_cast<int>(h);
            const ImU32 base   = handle_colour(handle.kind);
            const ImU32 colour = active ? active_colour : (hovered ? hover_colour : base);
            const float radius = active ? 7.0f : (hovered ? 6.0f : 4.5f);
            const ImVec2 p     = handle.point;
            if (handle.kind == 4) {
                // Vertices: solid dot, no tangent line. Moves the polygon skeleton.
                d->AddCircleFilled(p, radius, colour);
                if (active || hovered) d->AddCircle(p, radius + 2.0f, colour);
            } else if (handle.kind <= 1) {
                // Edge handles: diamond marker with a tangent line back to the edge end.
                d->AddLine(handle.anchor, p, active ? active_colour : IM_COL32(255, 140, 210, 130));
                const float size = active ? 6.0f : (hovered ? 5.5f : 4.5f);
                d->AddQuadFilled({p.x, p.y - size}, {p.x + size, p.y}, {p.x, p.y + size}, {p.x - size, p.y}, colour);
                if (active || hovered)
                    d->AddQuad({p.x, p.y - size - 1.5f}, {p.x + size + 1.5f, p.y}, {p.x, p.y + size + 1.5f},
                               {p.x - size - 1.5f, p.y}, colour);
            } else {
                // Corner tips: circle marker with a tangent line back to the corner end.
                d->AddLine(handle.anchor, p, active ? active_colour : IM_COL32(120, 215, 255, 130));
                d->AddCircleFilled(p, radius, colour);
                if (active || hovered) d->AddCircle(p, radius + 2.0f, colour);
            }
        }

        // Floating label next to the hovered or active handle, kept inside the viewport.
        int label_handle = hovered_handle;
        if (label_handle < 0 && editor.drag_handle_node == editor.selection && editor.drag_handle_kind >= 0)
            for (size_t h = 0; h < canvas_handle_count; ++h)
                if (canvas_handles[h].kind == editor.drag_handle_kind &&
                    canvas_handles[h].index == editor.drag_handle_index) {
                    label_handle = static_cast<int>(h);
                    break;
                }
        if (label_handle >= 0 && handle_tip[0] != '\0') {
            const ImVec2 anchor = canvas_handles[label_handle].point;
            const ImVec2 size   = ImGui::CalcTextSize(handle_tip);
            ImVec2 box_min{anchor.x + 14.0f, anchor.y - size.y - 8.0f};
            ImVec2 box_max{box_min.x + size.x + 10.0f, box_min.y + size.y + 6.0f};
            const ImVec2 view_min{o.x, o.y};
            const ImVec2 view_max{o.x + kDisplayWidth * z, o.y + kDisplayHeight * z};
            if (box_max.x > view_max.x) {
                const float shift = box_max.x - view_max.x;
                box_min.x -= shift;
                box_max.x -= shift;
            }
            if (box_min.x < view_min.x) {
                const float shift = view_min.x - box_min.x;
                box_min.x += shift;
                box_max.x += shift;
            }
            if (box_min.y < view_min.y) {
                const float shift = view_min.y - box_min.y;
                box_min.y += shift;
                box_max.y += shift;
            }
            d->AddRectFilled(box_min, box_max, IM_COL32(18, 22, 30, 235), 4.0f);
            d->AddRect(box_min, box_max, IM_COL32(120, 215, 255, 190), 4.0f);
            d->AddText({box_min.x + 5.0f, box_min.y + 3.0f}, IM_COL32(235, 240, 245, 255), handle_tip);
        }
    }
    // --- Control-handle drag ------------------------------------------------------
    // Invalidate a drag whose handle set no longer exists (overlay toggled off, selection
    // moved, node deleted, or the sample became invalid). Without this a phantom drag can
    // resume later from whatever the mouse happens to be over.
    if (editor.drag_handle_node >= 0 && (!handle_edit_ready || editor.drag_handle_node != editor.selection ||
                                         editor.drag_handle_node >= static_cast<int>(document.count))) {
        editor.drag_handle_node  = -1;
        editor.drag_handle_kind  = -1;
        editor.drag_handle_index = -1;
    }
    // Runs regardless of window hover so releasing outside the viewport ends cleanly.
    if (handle_edit_ready && editor.drag_handle_node >= 0 && editor.drag_handle_node == editor.selection) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const Vec2 world{(mouse.x - o.x) / z, (mouse.y - o.y) / z};
            const Vec2 local           = transformPoint(handle_world_inverse, world);
            Node& node                 = document.nodes[editor.drag_handle_node];
            const uint8_t vertex_count = handle_sample.parameters.vertex_count;
            const bool snap            = ImGui::GetIO().KeyShift;
            // Clamp after snapping: rounding can push a value past its bound (0.45 snaps to
            // 0.46 at step 0.02), which the preset compiler would then reject.
            const auto snap_clamp = [snap](float value, float step, float low, float high) {
                const float snapped = snap ? std::round(value / step) * step : value;
                return std::clamp(snapped, low, high);
            };
            // Reject a drag that would leave the polygon unsampleable (non-convex, degenerate
            // edge, overlapping trims) and restore the last valid authored state. Without
            // this the handles vanish mid-drag and the drag state gets stuck.
            const auto commit_or_restore = [&](auto&& restore) {
                RoundedPolygonSample candidate;
                if (sampleRoundedPolygon(node.polygon, worlds[editor.drag_handle_node], candidate, node.style)) {
                    changed = true;
                } else {
                    restore();
                    if (editor.status != "Drag rejected: keep the polygon valid")
                        editor.status = "Drag rejected: keep the polygon valid";
                }
            };
            if (editor.drag_handle_index >= 0 && editor.drag_handle_index < vertex_count) {
                const auto& parameters = handle_sample.parameters;
                if (editor.drag_handle_kind == 4) {
                    // Vertex handle: move the polygon skeleton directly, bounded to the same
                    // coordinate range the inspector allows. Without the bound an extreme drag
                    // can grow the sampled path until it breaks rendering or the sampler, which
                    // makes the handles appear to vanish.
                    auto& polygon       = node.polygon;
                    const Vec2 previous = polygon.vertices[editor.drag_handle_index];
                    Vec2 next           = snap ? Vec2{std::round(local.x), std::round(local.y)} : local;
                    if (!std::isfinite(next.x) || !std::isfinite(next.y)) next = previous;
                    next.x = std::clamp(next.x, kMinNodeCoordinate, kMaxNodeCoordinate);
                    next.y = std::clamp(next.y, kMinNodeCoordinate, kMaxNodeCoordinate);
                    polygon.vertices[editor.drag_handle_index] = next;
                    commit_or_restore([&] { polygon.vertices[editor.drag_handle_index] = previous; });
                } else if (editor.drag_handle_kind <= 1) {
                    // Edge handle. The "in" control sits at edge.start + normalize(dir +
                    // outward*bulge)*(start*L); the "out" control sits at edge.end -
                    // normalize(dir - outward*bulge)*(end*L). Each anchor therefore needs its
                    // own outward sign, otherwise the "out" drag measures backwards and pins
                    // itself to the clamp limits instead of following the cursor.
                    const auto& edge     = handle_sample.edges[editor.drag_handle_index];
                    const Vec2 edge_vec  = vsub(edge.end, edge.start);
                    const float edge_len = vlen(edge_vec);
                    if (edge_len > 1e-4f) {
                        const Vec2 dir             = vnorm(edge_vec);
                        const Vec2 outward         = {dir.y, -dir.x};
                        const bool from_start      = editor.drag_handle_kind == 0;
                        const Vec2 outward_eff     = from_start ? outward : Vec2{-outward.x, -outward.y};
                        const Vec2 offset          = from_start ? vsub(local, edge.start) : vsub(edge.end, local);
                        const float angle          = std::atan2(vdot(offset, outward_eff), vdot(offset, dir));
                        const float max_angle      = std::atan(kMaxEdgeBulge);
                        const float bulge          = std::tan(std::clamp(angle, -max_angle, max_angle));
                        const Vec2 tangent         = vnorm(vadd(dir, vscale(outward_eff, bulge)));
                        const float reach          = vdot(offset, tangent);
                        auto& edge_parameters      = node.polygon.edges[editor.drag_handle_index];
                        const float previous_bulge = edge_parameters.bulge;
                        const float previous_trim  = from_start ? edge_parameters.start : edge_parameters.end;
                        edge_parameters.bulge      = snap_clamp(bulge, 0.05f, -kMaxEdgeBulge, kMaxEdgeBulge);
                        (from_start ? edge_parameters.start : edge_parameters.end) =
                            snap_clamp(reach / edge_len, 0.02f, kMinEdgeHandle, kMaxEdgeHandle);
                        commit_or_restore([&] {
                            edge_parameters.bulge                                      = previous_bulge;
                            (from_start ? edge_parameters.start : edge_parameters.end) = previous_trim;
                        });
                    }
                } else {
                    // Corner handle: first_control = vertex + d * (u_prev + t_end * c) and
                    // second_control = vertex + d * (u_next - t_start * c), with
                    // d = min_neighbour_length * roundness. Solving the 2x2 system recovers
                    // both roundness and cap_tension, so the tip follows the cursor exactly
                    // while corner tangent authority is preserved.
                    const bool first_side   = editor.drag_handle_kind == 2;
                    const auto& corner      = handle_sample.corners[editor.drag_handle_index];
                    const Vec2 vertex       = parameters.vertices[editor.drag_handle_index];
                    const size_t count      = parameters.vertex_count;
                    const Vec2 previous     = parameters.vertices[(editor.drag_handle_index + count - 1) % count];
                    const Vec2 following    = parameters.vertices[(editor.drag_handle_index + 1) % count];
                    const float neighbour   = std::min(vlen(vsub(previous, vertex)), vlen(vsub(following, vertex)));
                    const Vec2 u            = vnorm(vsub(first_side ? corner.start : corner.end, vertex));
                    const Vec2 t            = vnorm(first_side ? vsub(corner.first_control, corner.start)
                                                               : vsub(corner.end, corner.second_control));
                    const Vec2 target       = vsub(local, vertex);
                    const float determinant = vcross(u, t);
                    if (neighbour > 1e-4f && std::fabs(determinant) > 1e-5f) {
                        const float distance = vcross(target, t) / determinant;
                        const float roundness =
                            snap_clamp(distance / neighbour, 0.01f, kMinCornerRoundness, kMaxCornerRoundness);
                        const float scaled = roundness * neighbour;
                        float tension      = kDefaultCapTension;
                        if (scaled > 1e-4f)
                            tension = (first_side ? 1.0f : -1.0f) * vdot(vsub(target, vscale(u, scaled)), t) / scaled;
                        auto& corner_parameters        = node.polygon.corners[editor.drag_handle_index];
                        const float previous_roundness = corner_parameters.roundness;
                        const float previous_tension   = corner_parameters.cap_tension;
                        corner_parameters.roundness    = roundness;
                        corner_parameters.cap_tension  = snap_clamp(tension, 0.05f, kMinCapTension, kMaxCapTension);
                        commit_or_restore([&] {
                            corner_parameters.roundness   = previous_roundness;
                            corner_parameters.cap_tension = previous_tension;
                        });
                    }
                }
                describe_handle(editor.drag_handle_kind, editor.drag_handle_index, handle_tip, sizeof(handle_tip));
            }
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            editor.drag_handle_node  = -1;
            editor.drag_handle_kind  = -1;
            editor.drag_handle_index = -1;
        }
    }

    if (hierarchy_valid && ImGui::IsWindowHovered()) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const ImVec2 mouse  = ImGui::GetIO().MousePos;
            bool started_handle = false;
            if (handle_edit_ready && hovered_handle >= 0 && hovered_handle < static_cast<int>(canvas_handle_count)) {
                const CanvasHandle& handle = canvas_handles[hovered_handle];
                editor.drag_handle_node    = editor.selection;
                editor.drag_handle_kind    = handle.kind;
                editor.drag_handle_index   = handle.index;
                editor.drag_node           = -1;
                started_handle             = true;
            }
            if (!started_handle) {
                const Vec2 point{(mouse.x - o.x) / z, (mouse.y - o.y) / z};
                bool hit = false;
                for (int index = int(display.count) - 1; index >= 0; --index) {
                    ShapePath path;
                    if (display.nodes[index].visible && sampleNode(display.nodes[index], worlds[index], path) &&
                        path.bounds.valid && point.x >= path.bounds.minimum.x && point.x <= path.bounds.maximum.x &&
                        point.y >= path.bounds.minimum.y && point.y <= path.bounds.maximum.y) {
                        editor.selection = index;
                        editor.drag_node = index;
                        hit              = true;
                        break;
                    }
                }
                if (!hit) editor.drag_node = -1;
            }
        }
        if (editor.drag_handle_node < 0 && editor.drag_node >= 0 &&
            editor.drag_node < static_cast<int>(document.count)) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                if (delta.x != 0.0f || delta.y != 0.0f) {
                    Node& node = document.nodes[editor.drag_node];
                    node.transform.translation.x += delta.x / z;
                    node.transform.translation.y += delta.y / z;
                    changed = true;
                }
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) editor.drag_node = -1;
        }
        // Right-click a handle to restore just that handle's parameters to defaults.
        if (handle_edit_ready && editor.drag_handle_node < 0 && hovered_handle >= 0 &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            const CanvasHandle& handle = canvas_handles[hovered_handle];
            Node& node                 = document.nodes[editor.selection];
            if (handle.kind <= 1) {
                auto& edge                                 = node.polygon.edges[handle.index];
                edge.bulge                                 = kDefaultEdgeBulge;
                (handle.kind == 0 ? edge.start : edge.end) = kDefaultEdgeHandle;
                changed                                    = true;
                editor.status                              = "Reset edge handle " + std::to_string(handle.index);
            } else if (handle.kind >= 2 && handle.kind <= 3) {
                auto& corner       = node.polygon.corners[handle.index];
                corner.roundness   = kDefaultCornerRoundness;
                corner.cap_tension = kDefaultCapTension;
                changed            = true;
                editor.status      = "Reset corner handle " + std::to_string(handle.index);
            } else {
                editor.status = "Vertex position has no default; drag or edit it in the inspector";
            }
        }
    }
    char zoom_label[32];
    snprintf(zoom_label, sizeof(zoom_label), "%.0f%%", editor.zoom * 100.0f);
    ImGui::Dummy(a);
    ImGui::EndChild();
    ImGui::TextDisabled("320 x 240 · %s · middle-drag pan · wheel zoom · shift snaps", zoom_label);
    ImGui::SameLine();
    ImGui::TextDisabled("| blue vertex · magenta edge handle · cyan corner tip · right-click resets a handle");
    if (editor.motion.playing) {
        ImGui::TextDisabled("| handles hidden while the preview clock runs — pause to edit");
    } else if (handle_tip[0] != '\0') {
        ImGui::TextUnformatted(handle_tip);
    }
    return changed;
}

}  // namespace kiwi::studio
