#include <pet/face/face_drawing_mailbox.h>

#include <cassert>

namespace {

pet::face::FaceDrawing drawingWithPathCount(uint8_t path_count)
{
    pet::face::FaceDrawing drawing{};
    drawing.path_count = path_count;
    return drawing;
}

}  // namespace

int main()
{
    pet::face::FaceDrawingMailbox mailbox;
    pet::face::FaceDrawingMailbox::ReadLease lease;

    assert(!mailbox.consumeLatest(lease));

    assert(mailbox.publish(drawingWithPathCount(1)));
    assert(mailbox.publish(drawingWithPathCount(2)));
    assert(mailbox.consumeLatest(lease));
    assert(lease.drawing->path_count == 2);

    // A slot being rendered is never overwritten; the next consumer still
    // receives the newest state published while that lease was held.
    assert(mailbox.publish(drawingWithPathCount(3)));
    assert(mailbox.publish(drawingWithPathCount(4)));
    assert(mailbox.publish(drawingWithPathCount(5)));
    mailbox.release(lease);

    assert(mailbox.consumeLatest(lease));
    assert(lease.drawing->path_count == 5);
    mailbox.release(lease);
    assert(!mailbox.consumeLatest(lease));
}
