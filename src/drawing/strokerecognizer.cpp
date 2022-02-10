#include <cmath>
#include "src/drawing/strokerecognizer.h"
#include "src/drawing/fullgraphicspath.h"
#include "src/preferences.h"

qreal distance(const QPointF &p) noexcept
{
   return std::sqrt(p.x()*p.x() + p.y()*p.y());
}

void StrokeRecognizer::calc1() noexcept
{
    if (stroke->type() == FullGraphicsPath::Type)
    {
        const FullGraphicsPath *path = static_cast<const FullGraphicsPath*>(stroke);
        QVector<float>::const_iterator pit = path->pressures.cbegin();
        QVector<QPointF>::const_iterator cit = path->coordinates.cbegin();
        for (; cit!=path->coordinates.cend() && pit!=path->pressures.cend(); ++pit, ++cit)
        {
            s += *pit;
            sx += *pit * cit->x();
            sy += *pit * cit->y();
            sxx += *pit * cit->x() * cit->x();
            sxy += *pit * cit->x() * cit->y();
            syy += *pit * cit->y() * cit->y();
        }
    }
    else
    {
        for (const QPointF &p : stroke->coordinates)
        {
            s += 1;
            sx += p.x();
            sy += p.y();
            sxx += p.x() * p.x();
            sxy += p.x() * p.y();
            syy += p.y() * p.y();
        }
    }
}

void StrokeRecognizer::calc2() noexcept
{
    if (stroke->type() == FullGraphicsPath::Type)
    {
        const FullGraphicsPath *path = static_cast<const FullGraphicsPath*>(stroke);
        QVector<float>::const_iterator pit = path->pressures.cbegin();
        QVector<QPointF>::const_iterator cit = path->coordinates.cbegin();
        for (; cit!=path->coordinates.cend() && pit!=path->pressures.cend(); ++pit, ++cit)
        {
            sxxx += *pit * cit->x() * cit->x() * cit->x();
            sxxy += *pit * cit->x() * cit->x() * cit->y();
            sxyy += *pit * cit->x() * cit->y() * cit->y();
            syyy += *pit * cit->y() * cit->y() * cit->y();
            sxxxx += *pit * cit->x() * cit->x() * cit->x() * cit->x();
            sxxyy += *pit * cit->x() * cit->x() * cit->y() * cit->y();
            syyyy += *pit * cit->y() * cit->y() * cit->y() * cit->y();
        }
    }
    else
    {
        for (const QPointF &p : stroke->coordinates)
        {
            sxxx += p.x() * p.x() * p.x();
            sxxy += p.x() * p.x() * p.y();
            sxyy += p.x() * p.y() * p.y();
            syyy += p.y() * p.y() * p.y();
            sxxxx += p.x() * p.x() * p.x() * p.x();
            sxxyy += p.x() * p.x() * p.y() * p.y();
            syyyy += p.y() * p.y() * p.y() * p.y();
        }
    }
}

BasicGraphicsPath *StrokeRecognizer::recognize()
{
    calc1();
    BasicGraphicsPath *path = recognizeLine();
    if (path)
        return path;
    path = recognizeRect();
    if (path)
        return path;
    calc2();
    path = recognizeEllipse();
    return path;
}

BasicGraphicsPath *StrokeRecognizer::recognizeLine() const
{
    if (stroke->size() < 3 || s == 0.)
        return NULL;
    const qreal
            n = sy*sy - s*syy + s*sxx - sx*sx,
            bx = sx/s, // x component of center of the line
            by = sy/s, // y component of center of the line
            d = 2*(sx*sy - s*sxy),
            ay = n - std::sqrt(n*n + d*d), // dx/dy = ay/d
            loss = (d*d*(s*syy-sy*sy) + ay*ay*(s*sxx-sx*sx) + 2*d*ay*(sx*sy-s*sxy))/((d*d+ay*ay) * (s*sxx - sx*sx + s*syy - sy*sy)),
            margin = stroke->_tool.width();
    debug_msg(DebugDrawing, "recognize line:" << bx << by << ay << d << loss)
    if (loss > preferences()->line_sensitivity)
        return NULL;

    QPointF p1, p2;
    if (std::abs(d) < std::abs(ay))
    {
        if (std::abs(d) < preferences()->snap_angle*std::abs(ay))
        {
            p1 = {bx, stroke->top + margin};
            p2 = {bx, stroke->bottom - margin};
        }
        else
        {
            p1 = {bx + d/ay*(stroke->top + margin - by), stroke->top + margin};
            p2 = {bx + d/ay*(stroke->bottom - margin - by), stroke->bottom - margin};
        }
    }
    else
    {
        if (std::abs(ay) < preferences()->snap_angle*std::abs(d))
        {
            p1 = {stroke->left + margin, by};
            p2 = {stroke->right - margin, by};
        }
        else
        {
            p1 = {stroke->left + margin, by + ay/d*(stroke->left + margin - bx)};
            p2 = {stroke->right - margin, by + ay/d*(stroke->right - margin - bx)};
        }
    }
    const int segments = distance(p1-p2)/10 + 2;
    const QPointF delta = (p2-p1)/segments;
    QVector<QPointF> coordinates(segments+1);
    for (int i=0; i<=segments; ++i)
        coordinates[i] = p1 + i*delta;
    const QRectF boundingRect(std::min(p1.x(), p2.x()) - margin, std::min(p1.y(), p2.y()) - margin, std::abs(p2.x()-p1.x()) + 2*margin, std::abs(p2.y()-p1.y()) + 2*margin);
    debug_msg(DebugDrawing, "recognized line" << p1 << p2);
    if (stroke->type() == FullGraphicsPath::Type)
    {
        DrawTool tool(stroke->_tool);
        tool.setWidth(s/stroke->size());
        return new BasicGraphicsPath(tool, coordinates, boundingRect);
    }
    return new BasicGraphicsPath(stroke->_tool, coordinates, boundingRect);
}


BasicGraphicsPath *StrokeRecognizer::recognizeRect() const
{
    return NULL;
}


BasicGraphicsPath *StrokeRecognizer::recognizeEllipse() const
{
    qreal ax, ay, rx, ry, mx, my, loss, grad_ax, grad_ay, grad_mx, grad_my, anorm, mnorm;
    const QPointF center = stroke->boundingRect().center();
    mx = center.x();
    my = center.y();
    rx = (stroke->right - stroke->left)/2;
    ry = (stroke->bottom - stroke->top)/2;
    debug_msg(DebugDrawing, "try to recognized ellipse" << mx << my << rx << ry);
    ax = 1./(rx*rx);
    ay = 1./(ry*ry);
    for (int i=0; i<8; ++i)
    {
        grad_ax = ellipseLossGradient_ax(mx, my, ax, ay);
        grad_ay = ellipseLossGradient_ay(mx, my, ax, ay);
        grad_mx = ellipseLossGradient_mx(mx, my, ax, ay);
        grad_my = ellipseLossGradient_my(mx, my, ax, ay);
        debug_verbose(DebugDrawing, grad_mx*(rx+ry)/s << grad_my*(rx+ry)/s << grad_ax*ax/s << grad_ay*ay/s);
        if (std::abs(grad_mx)*(rx+ry) < 1e-3*s && std::abs(grad_my)*(rx+ry) < 1e-3*s && std::abs(grad_ax)*ax < 1e-3*s && std::abs(grad_ay)*ay < 1e-3*s)
            break;
        // TODO: reasonable choice of step size
        mnorm = 0.05/((1+i*i)*std::sqrt(grad_mx*grad_mx + grad_my*grad_my));
        anorm = 0.1/((1+i*i)*std::sqrt(grad_ax*grad_ax + grad_ay*grad_ay));
        mx -= (rx+ry)*mnorm*grad_mx;
        my -= (rx+ry)*mnorm*grad_my;
        ax -= ax*anorm*grad_ax;
        ay -= ay*anorm*grad_ay;
        debug_verbose(DebugDrawing, mx << my << ax << ay << 1./std::sqrt(std::abs(ax)) << 1./std::sqrt(std::abs(ay)) << ellipseLossFunc(mx, my, ax, ay) / s);
    }
    loss = ellipseLossFunc(mx, my, ax, ay) / (s + 10);
    debug_msg(DebugDrawing, "    found:" << mx << my << 1./std::sqrt(ax) << 1./std::sqrt(ay) << loss);
    if (loss > preferences()->ellipse_sensitivity)
        return NULL;
    rx = 1./std::sqrt(ax);
    ry = 1./std::sqrt(ay);
    if (std::abs(rx - ry) < preferences()->ellipse_to_circle_snapping*(rx+ry))
        rx = ry = (rx+ry)/2;
    const int segments = (rx + ry) * 0.67 + 10;
    const qreal
            phasestep = 2*M_PI / segments,
            margin = stroke->_tool.width();
    QVector<QPointF> coordinates(segments+1);
    for (int i=0; i<segments; ++i)
        coordinates[i] = {mx + rx*std::sin(phasestep*i), my + ry*std::cos(phasestep*i)};
    coordinates[segments] = {mx, my + ry};
    const QRectF boundingRect(mx-rx-margin, my-ry-margin, 2*(rx+margin), 2*(ry+margin));
    debug_msg(DebugDrawing, "recognized ellipse" << mx << my << rx << ry << loss);
    if (stroke->type() == FullGraphicsPath::Type)
    {
        DrawTool tool(stroke->_tool);
        tool.setWidth(s/stroke->size());
        return new BasicGraphicsPath(tool, coordinates, boundingRect);
    }
    return new BasicGraphicsPath(stroke->_tool, coordinates, boundingRect);
}
