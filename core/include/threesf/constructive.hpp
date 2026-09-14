// Convex hulls, regularized solid Booleans, and self-intersection checks.
#pragma once
#include "threesf.hpp"
#include <memory>
#include <set>

namespace threesf {
namespace detail {
inline void check_tolerance(double tol) {
    if (!std::isfinite(tol) || tol <= 0) throw std::invalid_argument("tolerance must be finite and positive");
}
inline bool finite(const Vec3& p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
inline bool near(const Vec3& a, const Vec3& b, double eps) { return dist(a, b) <= eps; }
inline bool on_segment(const Vec3& p, const Vec3& a, const Vec3& b, double eps) {
    return near(p, closest_point_on_segment(p, a, b), eps);
}
inline bool segments_touch(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, double eps) {
    if (on_segment(a,c,d,eps) || on_segment(b,c,d,eps) || on_segment(c,a,b,eps) || on_segment(d,a,b,eps)) return true;
    Vec3 u = b-a, v = d-c, n = cross(u,v);
    double nn = norm2(n);
    if (nn == 0) return false;
    double t = dot(cross(c-a,v),n)/nn, s = dot(cross(c-a,u),n)/nn;
    return t >= 0 && t <= 1 && s >= 0 && s <= 1 && near(a+u*t,c+v*s,eps);
}
inline bool ring_crosses(const Ring& r, double eps) {
    std::size_t n = ring_size(r);
    for (std::size_t i=0; i<n; ++i) {
        if (!finite(r[i])) throw std::invalid_argument("non-finite coordinate");
        Vec3 a=r[i], b=r[(i+1)%n];
        for (std::size_t j=i+1; j<n; ++j) {
            Vec3 c=r[j], d=r[(j+1)%n];
            if (j == i+1 || (i == 0 && j == n-1)) {
                // Adjacent edges may share an endpoint, but may not backtrack.
                Vec3 shared=j==i+1 ? b : a, x=j==i+1 ? a : b, y=j==i+1 ? d : c;
                if (!near(x,shared,eps) && !near(y,shared,eps) &&
                    (on_segment(x,shared,y,eps) || on_segment(y,shared,x,eps))) return true;
            } else if (segments_touch(a,b,c,d,eps)) return true;
        }
    }
    return false;
}
inline bool rings_touch(const Ring& a, const Ring& b, double eps) {
    for (std::size_t i=0; i<ring_size(a); ++i)
        for (std::size_t j=0; j<ring_size(b); ++j)
            if (segments_touch(a[i],a[(i+1)%ring_size(a)],b[j],b[(j+1)%ring_size(b)],eps)) return true;
    return false;
}
// Boundary-inclusive point test, in the triangle's own plane.
inline bool in_triangle(const Vec3& p, const Triangle& t, double eps) {
    Vec3 n = cross(t.b-t.a,t.c-t.a);
    double len = norm(n);
    if (len == 0) return false;
    n = n/len;
    if (std::fabs(dot(n,p-t.a)) > eps) return false;
    return dot(n,cross(t.b-t.a,p-t.a)) >= -eps*dist(t.a,t.b) &&
           dot(n,cross(t.c-t.b,p-t.b)) >= -eps*dist(t.b,t.c) &&
           dot(n,cross(t.a-t.c,p-t.c)) >= -eps*dist(t.c,t.a);
}
// Intersections are allowed only on a shared vertex or shared edge. Testing
// only disjoint triangle pairs would miss folded adjacent and duplicate faces.
inline bool triangles_cross(const Triangle& first, const Triangle& second, double eps) {
    // Compute intersections near zero, so adding a hit to a large world
    // coordinate cannot move it off an otherwise exactly shared edge.
    Vec3 origin=first.a;
    Triangle a{first.a-origin,first.b-origin,first.c-origin};
    Triangle b{second.a-origin,second.b-origin,second.c-origin};
    Ring av{a.a,a.b,a.c}, bv{b.a,b.b,b.c}, shared;
    if (!bbox(av).expanded(eps).intersects(bbox(bv))) return false;
    for (auto& p : av) for (auto& q : bv) if (near(p,q,eps)) { shared.push_back(p); break; }
    if (shared.size() == 3) return true;
    auto forbidden = [&](const Vec3& p) {
        if (shared.size() == 1 && near(p,shared[0],eps)) return false;
        if (shared.size() == 2 && on_segment(p,shared[0],shared[1],eps)) return false;
        return true;
    };
    auto edges_hit = [&](const Ring& r, const Triangle& t, const Ring& tr) {
        Vec3 n = normalize(cross(t.b-t.a,t.c-t.a));
        for (std::size_t i=0; i<3; ++i) {
            Vec3 p=r[i], q=r[(i+1)%3];
            double dp=dot(n,p-t.a), dq=dot(n,q-t.a);
            if (in_triangle(p,t,eps) && forbidden(p)) return true;
            if ((dp > eps && dq < -eps) || (dp < -eps && dq > eps)) {
                Vec3 hit=p+(q-p)*(dp/(dp-dq));
                if (in_triangle(hit,t,eps) && forbidden(hit)) return true;
            }
            if (std::fabs(dp)<=eps && std::fabs(dq)<=eps) {
                if (in_triangle((p+q)*0.5,t,eps) && forbidden((p+q)*0.5)) return true;
                for (std::size_t j=0; j<3; ++j) {
                    Vec3 c=tr[j], d=tr[(j+1)%3], u=q-p, v=d-c, nn=cross(u,v);
                    double denom=norm2(nn);
                    if (denom == 0) continue;
                    double x=dot(cross(c-p,v),nn)/denom, y=dot(cross(c-p,u),nn)/denom;
                    if (x>=0 && x<=1 && y>=0 && y<=1) {
                        Vec3 hit=p+u*x;
                        if (near(hit,c+v*y,eps) && forbidden(hit)) return true;
                    }
                }
            }
        }
        return false;
    };
    return edges_hit(av,b,bv) || edges_hit(bv,a,av);
}
inline void gather_polygons(const Geometry& g, PolyhedralSurface& out) {
    out.patches.insert(out.patches.end(),g.polygons.begin(),g.polygons.end());
    for (auto& s:g.surfaces) out.patches.insert(out.patches.end(),s.patches.begin(),s.patches.end());
    for (auto& s:g.solids) {
        out.patches.insert(out.patches.end(),s.outer.patches.begin(),s.outer.patches.end());
        for (auto& h:s.inner) out.patches.insert(out.patches.end(),h.patches.begin(),h.patches.end());
    }
    for (auto& c:g.children) gather_polygons(c,out);
}
inline void gather_vertices(const Geometry& g, Ring& out) {
    out.insert(out.end(),g.points.begin(),g.points.end());
    for (auto& l:g.lines) out.insert(out.end(),l.pts.begin(),l.pts.end());
    auto poly=[&](const Polygon& p) {
        out.insert(out.end(),p.exterior.begin(),p.exterior.end());
        for (auto& h:p.holes) out.insert(out.end(),h.begin(),h.end());
    };
    for (auto& p:g.polygons) poly(p);
    for (auto& s:g.surfaces) for (auto& p:s.patches) poly(p);
    for (auto& s:g.solids) {
        for (auto& p:s.outer.patches) poly(p);
        for (auto& h:s.inner) for (auto& p:h.patches) poly(p);
    }
    for (auto& c:g.children) gather_vertices(c,out);
}
}

inline bool self_intersects(const Polygon& p, double tol) {
    detail::check_tolerance(tol);
    if (detail::ring_crosses(p.exterior,tol)) return true;
    for (std::size_t i=0; i<p.holes.size(); ++i) {
        if (detail::ring_crosses(p.holes[i],tol) || detail::rings_touch(p.exterior,p.holes[i],tol)) return true;
        for (std::size_t j=0; j<i; ++j) if (detail::rings_touch(p.holes[i],p.holes[j],tol)) return true;
    }
    return false;
}
inline bool self_intersects(const PolyhedralSurface& s, double tol) {
    detail::check_tolerance(tol);
    for (auto& p:s.patches) if (self_intersects(p,tol)) return true;
    std::vector<Triangle> tris;
    for (auto& p:s.patches) {
        auto ts=tessellate(p);
        tris.insert(tris.end(),ts.begin(),ts.end());
    }
    for (std::size_t i=0; i<tris.size(); ++i)
        for (std::size_t j=0; j<i; ++j)
            if (detail::triangles_cross(tris[i],tris[j],tol)) return true;
    return false;
}
inline bool self_intersects(const Solid& s, double tol) {
    PolyhedralSurface all=s.outer;
    for (auto& h:s.inner) all.patches.insert(all.patches.end(),h.patches.begin(),h.patches.end());
    return self_intersects(all,tol);
}
inline bool self_intersects(const Geometry& g, double tol) {
    PolyhedralSurface all; detail::gather_polygons(g,all);
    if (self_intersects(all,tol)) return true;
    for (auto& l:g.lines) {
        for (auto& p:l.pts) if (!detail::finite(p)) throw std::invalid_argument("non-finite coordinate");
        for (std::size_t i=1; i<l.pts.size(); ++i)
            for (std::size_t j=1; j+1<i; ++j) {
                if (j==1 && i+1==l.pts.size() && l.pts.front()==l.pts.back()) continue;
                if (detail::segments_touch(l.pts[i-1],l.pts[i],l.pts[j-1],l.pts[j],tol)) return true;
            }
    }
    for (auto& c:g.children) if (self_intersects(c,tol)) return true;
    return false;
}

namespace detail {
// Translate and scale before construction. All predicates below operate in
// this local frame; tol remains an absolute distance in the caller's units.
struct LocalFrame {
    Vec3 origin;
    double scale=1, eps=1e-9;
    LocalFrame(const Ring& points, double tol) {
        check_tolerance(tol);
        Box3 b;
        for (auto& p:points) { if (!finite(p)) throw std::invalid_argument("non-finite coordinate"); b.extend(p); }
        if (!b.empty()) {
            origin=b.min;
            Vec3 d=b.max-b.min;
            scale=std::max({d.x,d.y,d.z});
            if (scale==0) scale=1;
            if (!std::isfinite(scale)) throw std::invalid_argument("coordinate range overflow");
        }
        eps=std::max(tol/scale,64*std::numeric_limits<double>::epsilon());
    }
    Vec3 local(const Vec3& p) const { return (p-origin)/scale; }
    Vec3 world(const Vec3& p) const { return origin+p*scale; }
};
inline Ring unique_vertices(Ring points, double eps) {
    std::sort(points.begin(),points.end(),[](const Vec3& a,const Vec3& b){return key(a)<key(b);});
    Ring out;
    for (auto& p:points) {
        bool found=false;
        for (auto& q:out) if (near(p,q,eps)) { found=true; break; }
        if (!found) out.push_back(p);
    }
    return out;
}
inline Polygon triangle_polygon(const Triangle& t) { return {{t.a,t.b,t.c},{}}; }
}

// Any geometry contributes its vertices. Lower-dimensional hulls are returned
// as POINT, LINESTRING or POLYGON; full-dimensional hulls are SOLID.
inline Geometry convex_hull(const Geometry& g, double tol=1e-9) {
    Ring points; detail::gather_vertices(g,points);
    detail::LocalFrame frame(points,tol);
    if (self_intersects(g,tol)) detail::warn("self-intersection found in convex hull input; using its vertices");
    for (auto& p:points) p=frame.local(p);
    points=detail::unique_vertices(std::move(points),frame.eps);
    Geometry out; out.srid=g.srid;
    if (points.empty()) return out;
    auto point_result=[&] { out.type=GeomType::Point; out.points={frame.world(points[0])}; return out; };
    if (points.size()==1) return point_result();
    std::size_t b=1,c=0,d=0;
    for (std::size_t i=1;i<points.size();++i) if (norm2(points[i]-points[0])>norm2(points[b]-points[0])) b=i;
    Vec3 axis=normalize(points[b]-points[0]);
    double far=0;
    for (std::size_t i=0;i<points.size();++i) {
        double distance=norm(cross(axis,points[i]-points[0]));
        if (distance>far) {far=distance;c=i;}
    }
    if (far<=frame.eps) {
        auto mm=std::minmax_element(points.begin(),points.end(),[&](const Vec3& a,const Vec3& q){return dot(a,axis)<dot(q,axis);});
        out.type=GeomType::LineString; out.lines={LineString{{frame.world(*mm.first),frame.world(*mm.second)}}}; return out;
    }
    Vec3 normal=normalize(cross(points[b]-points[0],points[c]-points[0]));
    far=0;
    for (std::size_t i=0;i<points.size();++i) {
        double distance=std::fabs(dot(normal,points[i]-points[0]));
        if (distance>far) {far=distance;d=i;}
    }
    if (far<=frame.eps) {
        auto basis=plane_basis(normal);
        auto projected=detail::project(points,points[0],basis.first,basis.second,0);
        std::sort(projected.begin(),projected.end(),[](const auto& a,const auto& q){return std::tie(a.x,a.y)<std::tie(q.x,q.y);});
        std::vector<detail::V2> hull;
        for (auto& p:projected) {
            while(hull.size()>1 && detail::cross2(hull[hull.size()-2],hull.back(),p)<=0) hull.pop_back();
            hull.push_back(p);
        }
        std::size_t lower=hull.size();
        for (std::size_t i=projected.size()-1;i-->0;) {
            auto p=projected[i];
            while(hull.size()>lower && detail::cross2(hull[hull.size()-2],hull.back(),p)<=0) hull.pop_back();
            hull.push_back(p);
        }
        hull.pop_back();
        Polygon p; for(auto& v:hull) p.exterior.push_back(frame.world(points[v.idx]));
        out.type=GeomType::Polygon; out.polygons={p}; return out;
    }
    Vec3 inside=(points[0]+points[b]+points[c]+points[d])/4;
    std::vector<Triangle> faces;
    auto face=[&](Vec3 a,Vec3 q,Vec3 r) {
        if(dot(cross(q-a,r-a),inside-a)>0) std::swap(q,r);
        return Triangle{a,q,r};
    };
    faces={face(points[0],points[b],points[c]),face(points[0],points[d],points[b]),
           face(points[0],points[c],points[d]),face(points[b],points[d],points[c])};
    for(auto& p:points) {
        std::vector<Triangle> keep;
        std::map<std::pair<detail::VKey,detail::VKey>,std::pair<Vec3,Vec3>> horizon;
        for(auto& t:faces) {
            if(dot(normalize(cross(t.b-t.a,t.c-t.a)),p-t.a)<=frame.eps) {keep.push_back(t);continue;}
            Ring r{t.a,t.b,t.c};
            for(int i=0;i<3;++i) {
                Vec3 a=r[i],q=r[(i+1)%3]; auto rev=std::make_pair(detail::key(q),detail::key(a));
                auto it=horizon.find(rev);
                if(it!=horizon.end()) horizon.erase(it); else horizon[{detail::key(a),detail::key(q)}]={a,q};
            }
        }
        for(auto& e:horizon) keep.push_back(face(e.second.first,e.second.second,p));
        faces=std::move(keep);
    }
    Solid solid;
    for(auto& t:faces) solid.outer.patches.push_back(detail::triangle_polygon({frame.world(t.a),frame.world(t.b),frame.world(t.c)}));
    if(self_intersects(solid,tol)) detail::reject_self_intersection();
    if(!is_solid(solid)) throw std::runtime_error("convex hull: output is not manifold at this tolerance");
    out.type=GeomType::Solid; out.solids.push_back(std::move(solid)); return out;
}

namespace detail {
// Each BSP face is convex. Input polygons are triangulated before splitting.
struct BspPlane {
    Vec3 normal, anchor;
    explicit BspPlane(const Polygon& p): normal(unit_normal(p)), anchor(p.exterior[0]) {}
    void split(const Polygon& p, std::vector<Polygon>& coplanar_front, std::vector<Polygon>& coplanar_back,
               std::vector<Polygon>& front, std::vector<Polygon>& back, double eps) const {
        int mask=0; std::vector<int> sides;
        for(auto& v:p.exterior) {double d=dot(normal,v-anchor);int side=d>eps?1:(d< -eps?2:0);sides.push_back(side);mask|=side;}
        if(mask==0) { (dot(normal,unit_normal(p))>=0?coplanar_front:coplanar_back).push_back(p);return; }
        if(mask==1) {front.push_back(p);return;}
        if(mask==2) {back.push_back(p);return;}
        Polygon f,b;
        for(std::size_t i=0;i<p.exterior.size();++i) {
            std::size_t j=(i+1)%p.exterior.size();Vec3 v=p.exterior[i],w=p.exterior[j];
            if(sides[i]!=2) f.exterior.push_back(v);
            if(sides[i]!=1) b.exterior.push_back(v);
            if((sides[i]|sides[j])==3) {
                Vec3 hit=v+(w-v)*(dot(normal,anchor-v)/dot(normal,w-v));
                f.exterior.push_back(hit);b.exterior.push_back(hit);
            }
        }
        if(f.exterior.size()>=3 && norm(newell_normal(f.exterior))>eps*eps) front.push_back(std::move(f));
        if(b.exterior.size()>=3 && norm(newell_normal(b.exterior))>eps*eps) back.push_back(std::move(b));
    }
};
struct Bsp {
    double eps;
    std::unique_ptr<BspPlane> plane;
    std::vector<Polygon> faces;
    std::unique_ptr<Bsp> front,back;
    explicit Bsp(double tolerance):eps(tolerance) {}
    Bsp(const std::vector<Polygon>& polygons,double tolerance):eps(tolerance) {build(polygons);}
    void build(const std::vector<Polygon>& polygons) {
        if(polygons.empty()) return;
        if(!plane) plane=std::make_unique<BspPlane>(polygons[0]);
        std::vector<Polygon> f,b;
        for(auto& p:polygons) plane->split(p,faces,faces,f,b,eps);
        if(!f.empty()) {if(!front) front=std::make_unique<Bsp>(eps);front->build(f);}
        if(!b.empty()) {if(!back) back=std::make_unique<Bsp>(eps);back->build(b);}
    }
    std::vector<Polygon> all() const {
        std::vector<Polygon> out=faces;
        if(front) {auto ps=front->all();out.insert(out.end(),ps.begin(),ps.end());}
        if(back) {auto ps=back->all();out.insert(out.end(),ps.begin(),ps.end());}
        return out;
    }
    void invert() {
        for(auto& p:faces) p=reversed(p);
        if(plane) plane->normal=plane->normal*-1;
        if(front) front->invert();
        if(back) back->invert();
        std::swap(front,back);
    }
    std::vector<Polygon> clip(const std::vector<Polygon>& polygons) const {
        if(!plane) return polygons;
        std::vector<Polygon> f,b;
        for(auto& p:polygons) plane->split(p,f,b,f,b,eps);
        if(front) f=front->clip(f);
        if(back) b=back->clip(b); else b.clear();
        f.insert(f.end(),b.begin(),b.end());return f;
    }
    void clip_to(const Bsp& other) {
        faces=other.clip(faces);
        if(front) front->clip_to(other);
        if(back) back->clip_to(other);
    }
};
// Solid-angle containment avoids ambiguous rays through triangulation edges.
inline bool shell_contains(const PolyhedralSurface& s,const Vec3& p) {
    double angle=0;
    for(auto& t:tessellate(s)) {
        Vec3 a=t.a-p,b=t.b-p,c=t.c-p;
        double la=norm(a),lb=norm(b),lc=norm(c);
        angle+=2*std::atan2(dot(a,cross(b,c)),la*lb*lc+dot(a,b)*lc+dot(b,c)*la+dot(c,a)*lb);
    }
    return std::fabs(angle)>6.283185307179586;
}
// Weld roundoff differences and insert every vertex lying on a neighboring
// edge. BSP splitting otherwise leaves T-junctions in the output boundary.
inline PolyhedralSurface stitch(std::vector<Polygon> faces,double eps) {
    Ring vertices;
    for(auto& p:faces) for(auto& v:p.exterior) {
        auto it=std::find_if(vertices.begin(),vertices.end(),[&](const Vec3& q){return near(v,q,eps);});
        if(it==vertices.end()) vertices.push_back(v); else v=*it;
    }
    PolyhedralSurface out;
    for(auto& p:faces) {
        Ring clean;
        for(auto& v:p.exterior) if(clean.empty() || v!=clean.back()) clean.push_back(v);
        clean=open_ring(clean);
        if(clean.size()<3 || norm(newell_normal(clean))<=eps*eps) continue;
        Ring ring;
        for(std::size_t i=0;i<clean.size();++i) {
            Vec3 a=clean[i],b=clean[(i+1)%clean.size()];
            std::vector<std::pair<double,Vec3>> edge{{0,a}};
            for(auto& v:vertices) {
                if(v==a || v==b) continue;
                double t=dot(v-a,b-a)/norm2(b-a);
                if(t>0 && t<1 && on_segment(v,a,b,eps)) edge.push_back({t,v});
            }
            std::sort(edge.begin(),edge.end(),[](const auto& a,const auto& b){return a.first<b.first;});
            for(auto& v:edge) ring.push_back(v.second);
        }
        // A center fan preserves all collinear edge vertices. Ear clipping can
        // cut past a split vertex when its computed orientation rounds to zero.
        if (ring.size()==3) out.patches.push_back({std::move(ring),{}});
        else {
            Vec3 center=ring_centroid_vertices(ring);
            for(std::size_t i=0;i<ring.size();++i)
                out.patches.push_back({{ring[i],ring[(i+1)%ring.size()],center},{}});
        }
    }
    return out;
}
// Group edge-connected shells, then attach inward shells to their smallest
// containing outward shell. Disconnected regions remain separate solids.
inline std::vector<Solid> assemble(const PolyhedralSurface& mesh,double eps) {
    using Edge=std::pair<VKey,VKey>;
    std::map<Edge,std::vector<std::size_t>> edges;
    for(std::size_t i=0;i<mesh.patches.size();++i) {
        auto r=open_ring(mesh.patches[i].exterior);
        for(std::size_t j=0;j<r.size();++j) {
            auto a=key(r[j]),b=key(r[(j+1)%r.size()]);
            if(b<a) std::swap(a,b);
            edges[{a,b}].push_back(i);
        }
    }
    std::vector<std::vector<std::size_t>> adjacent(mesh.patches.size());
    for(auto& e:edges) {
        if(e.second.size()!=2) throw std::invalid_argument("Boolean: boundary is not manifold at this tolerance");
        auto a=e.second[0],b=e.second[1];adjacent[a].push_back(b);adjacent[b].push_back(a);
    }
    std::vector<bool> seen(mesh.patches.size());
    std::vector<Solid> solids;std::vector<PolyhedralSurface> cavities;
    for(std::size_t i=0;i<mesh.patches.size();++i) {
        if(seen[i]) continue;
        PolyhedralSurface shell;std::vector<std::size_t> todo{i};seen[i]=true;
        while(!todo.empty()) {
            auto j=todo.back();todo.pop_back();shell.patches.push_back(mesh.patches[j]);
            for(auto k:adjacent[j]) if(!seen[k]) {seen[k]=true;todo.push_back(k);}
        }
        if(!is_solid(shell)) throw std::invalid_argument("Boolean: boundary has inconsistent orientation");
        double v=signed_volume(shell);
        if(std::fabs(v)<=eps*eps*eps) throw std::invalid_argument("Boolean: degenerate shell");
        if(v>0) solids.push_back({std::move(shell),{}});else cavities.push_back(std::move(shell));
    }
    for(auto& h:cavities) {
        std::size_t owner=solids.size();double smallest=std::numeric_limits<double>::infinity();
        for(std::size_t i=0;i<solids.size();++i) {
            double v=volume(solids[i].outer);
            if(v<smallest && shell_contains(solids[i].outer,h.patches[0].exterior[0])) {smallest=v;owner=i;}
        }
        if(owner==solids.size()) throw std::invalid_argument("Boolean: cavity is outside every outer shell");
        solids[owner].inner.push_back(std::move(h));
    }
    auto contains=[&](const Solid& s,const Vec3& p) {
        if(!shell_contains(s.outer,p)) return false;
        for(auto& h:s.inner) if(shell_contains(h,p)) return false;
        return true;
    };
    for(std::size_t i=0;i<solids.size();++i)
        for(std::size_t j=0;j<i;++j)
            if(contains(solids[i],centroid(solids[j].outer.patches[0])) ||
               contains(solids[j],centroid(solids[i].outer.patches[0])))
                throw std::invalid_argument("Boolean: solid components have overlapping interiors");
    return solids;
}
inline PolyhedralSurface local_shell(const PolyhedralSurface& s,const LocalFrame& frame) {
    PolyhedralSurface out=s;
    for(auto& p:out.patches) {
        for(auto& v:p.exterior) v=frame.local(v);
        for(auto& h:p.holes) for(auto& v:h) v=frame.local(v);
        if(!is_planar(p,frame.eps) || norm(newell_normal(p.exterior))<=frame.eps*frame.eps)
            throw std::invalid_argument("Boolean: requires nondegenerate planar faces");
    }
    if(!is_solid(out)) throw std::invalid_argument("Boolean: requires closed, consistently oriented shells");
    return out;
}
inline void boolean_faces(const Geometry& g,const LocalFrame& frame,std::vector<Polygon>& out) {
    if(!g.points.empty() || !g.lines.empty() || !g.polygons.empty())
        throw std::invalid_argument("Boolean: operands must be solids or closed polyhedral surfaces");
    auto add=[&](const PolyhedralSurface& s,bool inward) {
        for(auto t:tessellate(s)) {
            if(inward) std::swap(t.b,t.c);
            out.push_back(triangle_polygon(t));
        }
    };
    for(auto& s:g.surfaces) {
        if(s.patches.empty()) continue;
        auto shell=force_outward(local_shell(s,frame));
        // A surface may represent several disjoint outer shells and cavities.
        auto parts=assemble(shell,frame.eps);
        for(auto& part:parts) {add(part.outer,false);for(auto& h:part.inner) add(force_outward(h),true);}
    }
    for(auto& s:g.solids) {
        if(s.outer.patches.empty() && s.inner.empty()) continue;
        auto outer=force_outward(local_shell(s.outer,frame));add(outer,false);
        std::vector<PolyhedralSurface> holes;
        for(auto& h:s.inner) {
            auto hole=force_outward(local_shell(h,frame));
            if(!shell_contains(outer,hole.patches[0].exterior[0])) throw std::invalid_argument("Boolean: cavity outside outer shell");
            for(auto& other:holes)
                if(shell_contains(other,hole.patches[0].exterior[0]) || shell_contains(hole,other.patches[0].exterior[0]))
                    throw std::invalid_argument("Boolean: nested cavities are invalid");
            add(hole,true);holes.push_back(std::move(hole));
        }
    }
    for(auto& c:g.children) boolean_faces(c,frame,out);
}
enum class BooleanOp { Intersection, Union, Difference };
inline Geometry boolean_operation(const Geometry& a,const Geometry& b,BooleanOp op,double tol) {
    if(a.srid!=b.srid) throw std::invalid_argument("Boolean: operands have different SRIDs");
    Ring vertices;gather_vertices(a,vertices);gather_vertices(b,vertices);
    LocalFrame frame(vertices,tol);
    if(self_intersects(a,tol) || self_intersects(b,tol)) reject_self_intersection();
    std::vector<Polygon> af,bf;
    boolean_faces(a,frame,af);boolean_faces(b,frame,bf);
    // Boundary intersection tests alone do not catch one solid nested wholly
    // inside another component. Such operands are not valid MULTISOLIDs.
    assemble(PolyhedralSurface{af},frame.eps);
    assemble(PolyhedralSurface{bf},frame.eps);
    std::vector<Polygon> result;
    if(af.empty() || bf.empty()) {
        if(op==BooleanOp::Union) result=af.empty()?bf:af;
        else if(op==BooleanOp::Difference) result=af;
    } else {
        Bsp left(af,frame.eps),right(bf,frame.eps);
        if(op==BooleanOp::Intersection) {
            left.invert();right.clip_to(left);right.invert();left.clip_to(right);right.clip_to(left);
            left.build(right.all());left.invert();
        } else if(op==BooleanOp::Union) {
            left.clip_to(right);right.clip_to(left);right.invert();right.clip_to(left);right.invert();left.build(right.all());
        } else {
            left.invert();left.clip_to(right);right.clip_to(left);right.invert();right.clip_to(left);right.invert();
            left.build(right.all());left.invert();
        }
        result=left.all();
    }
    auto mesh=stitch(std::move(result),frame.eps);
    if(self_intersects(mesh,frame.eps)) reject_self_intersection();
    Geometry out;out.srid=a.srid;
    if(mesh.patches.empty()) return out;
    out.solids=assemble(mesh,frame.eps);
    for(auto& s:out.solids) {
        auto world=[&](PolyhedralSurface& shell) {for(auto& p:shell.patches) for(auto& v:p.exterior) v=frame.world(v);};
        world(s.outer);for(auto& h:s.inner) world(h);
    }
    out.type=out.solids.size()==1?GeomType::Solid:GeomType::MultiSolid;
    if(self_intersects(out,tol)) reject_self_intersection();
    return out;
}
}

// Regularized volume operations: face/edge/point-only intersections are EMPTY.
// Unsupported dimensions and non-manifold results fail instead of returning
// an invalid mesh. union_ avoids the reserved C++ keyword.
inline Geometry intersection(const Geometry& a,const Geometry& b,double tol=1e-9) {
    return detail::boolean_operation(a,b,detail::BooleanOp::Intersection,tol);
}
inline Geometry union_(const Geometry& a,const Geometry& b,double tol=1e-9) {
    return detail::boolean_operation(a,b,detail::BooleanOp::Union,tol);
}
inline Geometry difference(const Geometry& a,const Geometry& b,double tol=1e-9) {
    return detail::boolean_operation(a,b,detail::BooleanOp::Difference,tol);
}
} // namespace threesf
