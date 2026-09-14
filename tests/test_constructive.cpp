#include "threesf/threesf.hpp"
#include "threesf/io.hpp"
#include "threesf/threesf_c.h"
#include <cstdio>
#include <functional>
#include <random>
#include <string>
using namespace threesf;
static int fails=0;
#define CHECK(c) do { if(!(c)) {std::printf("FAIL line %d: %s\n",__LINE__,#c);++fails;} } while(0)
#define NEAR(a,b) CHECK(std::isfinite(a) && std::fabs((a)-(b))<1e-7)
static Geometry box(double x,double y,double z,double dx=1,double dy=1,double dz=1) {
    Polygon p{{{x,y,z},{x+dx,y,z},{x+dx,y+dy,z},{x,y+dy,z}}, {}};
    Geometry g;g.type=GeomType::Solid;g.solids={extrude(p,{0,0,dz})};return g;
}
static void valid(const Geometry& g) {
    CHECK(!self_intersects(g));
    for(auto& s:g.solids) CHECK(is_solid(s));
    CHECK(write_wkt(read_wkb(write_wkb(g)))==write_wkt(g));
}
static void rejects(const std::function<void()>& f) {
    bool rejected=false;try {f();} catch(const std::exception&) {rejected=true;} CHECK(rejected);
}
static void run(const char* name,const std::function<void()>& f) {
    try {f();} catch(const std::exception& e) {std::printf("FAIL %s: %s\n",name,e.what());++fails;}
}
int main() {
    run("hull",[]{
        Geometry g=box(0,0,0);g.points={{0.5,0.5,0.5},{0,0,0}};
        auto h=convex_hull(g);valid(h);NEAR(volume(h),1);CHECK(h.type==GeomType::Solid);
        Geometry tetra;tetra.points={{0,0,0},{1,0,0},{0,1,0},{0,0,1}};
        auto t=convex_hull(tetra);NEAR(volume(t),1.0/6);valid(t);
        Geometry flat;flat.srid=4326;flat.points={{0,2,0},{2,2,0},{2,2,2},{0,2,2},{1,2,1}};
        auto p=convex_hull(flat);CHECK(p.type==GeomType::Polygon);NEAR(area(p),4);CHECK(p.srid==4326);
        Geometry line;line.points={{3,3,3},{1,1,1},{2,2,2},{1,1,1}};
        auto l=convex_hull(line);CHECK(l.type==GeomType::LineString);NEAR(length(l),std::sqrt(12.0));
        line.points={{1,1,1},{1,1,1}};CHECK(convex_hull(line).type==GeomType::Point);
        CHECK(convex_hull(Geometry{}).empty());
        auto shifted=convex_hull(box(1e8,-1e8,1e8));valid(shifted);
        auto small=convex_hull(box(0,0,0,1e-5,1e-5,1e-5));valid(small);
        rejects([&]{convex_hull(g,0);});
        g.points={{NAN,0,0}};rejects([&]{convex_hull(g);});
    });
    run("overlap",[]{
        auto a=box(0,0,0),b=box(.5,.5,.5);
        auto i=intersection(a,b),u=union_(a,b),d=difference(a,b);
        valid(i);valid(u);valid(d);NEAR(volume(i),.125);NEAR(volume(u),1.875);NEAR(volume(d),.875);
        valid(union_(d,i));NEAR(volume(union_(d,i)),1);
    });
    run("identity and contact",[]{
        auto a=box(0,0,0);
        valid(union_(a,a));NEAR(volume(union_(a,a)),1);NEAR(volume(intersection(a,a)),1);
        CHECK(difference(a,a).empty());
        for(auto b:{box(1,0,0),box(1,1,0),box(1,1,1),box(2,0,0)}) CHECK(intersection(a,b).empty());
        auto u=union_(a,box(1,0,0));valid(u);NEAR(volume(u),2);
        auto disjoint=union_(a,box(2,0,0));valid(disjoint);CHECK(disjoint.type==GeomType::MultiSolid);NEAR(volume(disjoint),2);
        CHECK(intersection(a,Geometry{}).empty());NEAR(volume(union_(a,Geometry{})),1);
        NEAR(volume(difference(a,Geometry{})),1);CHECK(difference(Geometry{},a).empty());
    });
    run("cavities and split",[]{
        auto a=box(0,0,0,4,4,4),b=box(1,1,1,2,2,2);
        auto d=difference(a,b);valid(d);NEAR(volume(d),56);CHECK(d.solids[0].inner.size()==1);
        CHECK(intersection(d,b).empty());NEAR(volume(union_(d,b)),64);
        auto split=difference(a,box(1,-1,-1,2,6,6));valid(split);CHECK(split.solids.size()==2);NEAR(volume(split),32);
        NEAR(volume(intersection(d,a)),56);
        NEAR(volume(difference(d,box(-1,-1,-1,3,6,6))),28);
    });
    run("nonconvex and rotated",[]{
        Polygon l{{{0,0,0},{2,0,0},{2,1,0},{1,1,0},{1,2,0},{0,2,0}}, {}};
        Geometry a;a.type=GeomType::Solid;a.solids={extrude(l,{0,0,2})};
        auto b=box(.5,.5,.5);auto i=intersection(a,b);valid(i);NEAR(volume(i),.75);
        auto rotated=box(0,0,0);
        for(auto& p:rotated.solids[0].outer.patches) for(auto& v:p.exterior) {
            double x=v.x-.5,y=v.y-.5;v.x=.5+(x-y)/std::sqrt(2.0);v.y=.5+(x+y)/std::sqrt(2.0);
        }
        auto unit=box(0,0,0);auto ri=intersection(unit,rotated),ru=union_(unit,rotated);
        valid(ri);valid(ru);NEAR(volume(ri),2*std::sqrt(2.0)-2);NEAR(volume(ru)+volume(ri),2);
    });
    run("self intersection",[]{
        Polygon bow{{{0,0,0},{2,2,0},{0,2,0},{2,0,0}}, {}};
        CHECK(self_intersects(bow));rejects([&]{tessellate(bow);});rejects([&]{extrude(bow,{0,0,1});});
        Polygon vertical=bow;for(auto& v:vertical.exterior) std::swap(v.y,v.z);CHECK(self_intersects(vertical));
        PolyhedralSurface crossing{{{{{0,0,0},{2,0,0},{0,2,0}},{}},{{{.5,.5,-1},{.5,.5,1},{2,.5,0}},{}}}};
        CHECK(self_intersects(crossing));
        PolyhedralSurface duplicate{{{{{0,0,0},{2,0,0},{0,2,0}},{}},{{{0,0,0},{2,0,0},{0,2,0}},{}}}};
        CHECK(self_intersects(duplicate));
        // Sharing a vertex does not exempt the rest of the pair from testing.
        PolyhedralSurface adjacent{{{{{0,0,0},{2,0,0},{0,2,0}},{}},{{{0,0,0},{1,0,0},{0,1,0}},{}}}};
        CHECK(self_intersects(adjacent));CHECK(!self_intersects(box(0,0,0)));
        auto bad=box(0,0,0);auto other=box(.5,.5,.5);bad.solids.push_back(other.solids[0]);
        CHECK(self_intersects(bad));rejects([&]{union_(bad,other);});
        auto open=box(0,0,0);open.solids[0].outer.patches.pop_back();rejects([&]{intersection(open,other);});
        Geometry p;p.points={{0,0,0}};rejects([&]{union_(p,other);});
        other.srid=4326;rejects([&]{difference(box(0,0,0),other);});
        Geometry nested; nested.type=GeomType::MultiSolid;
        nested.solids={box(0,0,0,4,4,4).solids[0],box(1,1,1).solids[0]};
        rejects([&]{union_(nested,Geometry{});});
    });
    run("random boxes",[]{
        std::mt19937 rng(42);std::uniform_real_distribution<double> coord(-.5,1.5);
        auto a=box(0,0,0);
        for(int j=0;j<20;++j) {
            double x=coord(rng),y=coord(rng),z=coord(rng);auto b=box(x,y,z);
            auto overlap=[](double x){return std::max(0.0,std::min(1.0,x+1)-std::max(0.0,x));};
            double expected=overlap(x)*overlap(y)*overlap(z);
            auto i=intersection(a,b),u=union_(a,b),d=difference(a,b);
            valid(i);valid(u);valid(d);NEAR(volume(i),expected);NEAR(volume(u),2-expected);NEAR(volume(d),1-expected);
        }
    });
    run("random hulls",[]{
        std::mt19937 rng(17);std::uniform_real_distribution<double> coord(-1,1);
        for(int trial=0;trial<10;++trial) {
            Geometry cloud;
            for(int i=0;i<30;++i) cloud.points.push_back({coord(rng),coord(rng),coord(rng)});
            auto hull=convex_hull(cloud);valid(hull);
            for(auto& face:hull.solids[0].outer.patches)
                for(auto& point:cloud.points) CHECK(dot(unit_normal(face),point-face.exterior[0])<=1e-9);
            auto reverse=cloud;std::reverse(reverse.points.begin(),reverse.points.end());
            NEAR(volume(convex_hull(reverse)),volume(hull));
        }
    });
    run("C ABI warnings and errors",[]{
        auto* bad=threesf_read_wkt("POLYGON Z ((0 0 0,2 2 0,0 2 0,2 0 0,0 0 0))");
        CHECK(bad);CHECK(std::string(threesf_last_warning()).find("self-intersection")!=std::string::npos);
        CHECK(threesf_h_self_intersects(bad,1e-9)==1);
        CHECK(threesf_h_tessellate(bad)==nullptr);CHECK(std::string(threesf_last_error()).find("self-intersection")!=std::string::npos);
        CHECK(std::isnan(threesf_h_area_tessellated(bad)));
        auto* h=threesf_h_convex_hull(bad,1e-9);CHECK(h);threesf_h_free(h);threesf_h_free(bad);
        CHECK(threesf_h_convex_hull(nullptr,1e-9)==nullptr);CHECK(std::string(threesf_last_error())=="null geometry");
        auto a=box(0,0,0),b=box(.5,0,0);
        auto* ah=threesf_read_wkt(write_wkt(a).c_str());auto* bh=threesf_read_wkt(write_wkt(b).c_str());
        auto* ih=threesf_h_intersection(ah,bh,1e-9);CHECK(ih);NEAR(threesf_h_volume(ih),.5);
        CHECK(std::string(threesf_last_warning()).empty());
        threesf_h_free(ih);threesf_h_free(ah);threesf_h_free(bh);
    });
    run("plot buffers",[]{
        auto* g=threesf_read_wkt("GEOMETRYCOLLECTION Z (POINT Z (4 5 6), LINESTRING Z (5 5 5,6 6 6), POLYGON Z ((0 0 0,2 0 0,2 2 0,0 2 0,0 0 0)))");
        double* data=nullptr;size_t counts[3]={0,0,0};
        CHECK(threesf_h_plot_data(g,1,&data,counts)==0);
        CHECK(counts[0]==1 && counts[1]==5 && counts[2]==2);
        CHECK(data && data[0]==4 && data[2]==6);threesf_free(data);
        CHECK(threesf_h_plot_data(g,0,&data,counts)==0);
        CHECK(counts[1]==1 && counts[2]==2);threesf_free(data);threesf_h_free(g);
        g=threesf_read_wkt("GEOMETRYCOLLECTION EMPTY");
        CHECK(threesf_h_plot_data(g,1,&data,counts)==0);
        CHECK(!data && counts[0]==0 && counts[1]==0 && counts[2]==0);threesf_h_free(g);
        CHECK(threesf_h_plot_data(nullptr,1,&data,counts)==-1);
        CHECK(threesf_h_plot_data(nullptr,1,nullptr,counts)==-1);
    });
    if(!fails) std::puts("constructive tests passed");
    return fails?1:0;
}
