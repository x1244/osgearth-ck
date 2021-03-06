/* -*-c++-*- */
/* osgEarth - Geospatial SDK for OpenSceneGraph
* Copyright 2020 Pelican Mapping
* http://osgearth.org
*
* osgEarth is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include <osg/ref_ptr>
#include <osg/Image>
#include <osgText/Text>
#include <osgViewer/Viewer>
#include <osgEarth/Controls>
#include <osgViewer/ViewerEventHandlers>

#include <osgEarth/Notify>
#include <osgEarth/EarthManipulator>
#include <osgEarth/ExampleResources>
#include <osgEarth/FeatureNode>
#include <osgEarth/GeometryFactory>
#include <osgEarth/LogarithmicDepthBuffer>
#include <osgEarth/MapNode>
#include <osgEarth/PlaceNode>
#include <osgEarth/LabelNode>
#include <osgEarth/ModelNode>
#include <osgEarth/ShaderGenerator>
#include <osgEarth/Registry>
#include <osgEarth/ThreadingUtils>
#include <osgDB/ReadFile>
#include <iostream>

#include <osgEarth/Metrics>
#include <iostream>
#include "earth_loader.h"
#include "panorama.h"
#include "panorama_camera.h"
#include "pick.h"

#define LC "[viewer] "

using namespace osgEarth;
using namespace osgEarth::Util;
using namespace std;
int
usage(const char* name)
{
    OE_NOTICE 
        << "\nUsage: " << name << " file.earth" << std::endl
        << MapNodeHelper().usage() << std::endl;

    return 0;
}

int main(int argc, char** argv)
{
    osg::ArgumentParser arguments(&argc,argv);

    // help?
    if ( arguments.read("--help") )
        return usage(argv[0]);
    // create a viewer:
    osgViewer::Viewer viewer(arguments);

    // Tell the database pager to not modify the unref settings
    viewer.getDatabasePager()->setUnrefImageDataAfterApplyPolicy( true, false );

    // thread-safe initialization of the OSG wrapper manager. Calling this here
    // prevents the "unsupported wrapper" messages from OSG
    osgDB::Registry::instance()->getObjectWrapperManager()->findWrapper("osg::Image");

    // install our default manipulator (do this before calling load)
    osg::ref_ptr<osgEarth::Util::EarthManipulator> earthManipulator = new EarthManipulator(arguments);
    viewer.setCameraManipulator(earthManipulator);

    // disable the small-feature culling
    viewer.getCamera()->setSmallFeatureCullingPixelSize(-1.0f);

    // set a near/far ratio that is smaller than the default. This allows us to get
    // closer to the ground without near clipping. If you need more, use --logdepth
    viewer.getCamera()->setNearFarRatio(0.0001);
    viewer.getCamera()->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF|osg::StateAttribute::OVERRIDE);

    
    osg::ref_ptr<osg::Group> root = new osg::Group;
	//osg::Node* model = osgDB::readNodeFile("../data/cow.osgt");
	//root->addChild(model);

    // load an earth file, and support all or our example command-line options
    // and earth file <external> tags   
//#define YYY 1	
#ifdef YYY
	osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(argv[1]);
	root->addChild(node);
#else
    osg::Node* node = MapNodeHelper().load(arguments, &viewer);
	cout <<"earth r:" <<node->getBound().radius() <<endl;
    if ( node )
    {
        root->addChild(node);
    }
    else
    {
        return usage(argv[0]);
    }
#endif

    // find the map node that we loaded.
    MapNode* mapNode = MapNode::findMapNode(node);
    if ( !mapNode )
        return usage(argv[0]);
    // Group to hold all our annotation elements.
    osg::Group* annoGroup = new osg::Group();
    MapNode::get(node)->addChild( annoGroup );

    // Make a group for labels
    osg::Group* labelGroup = new osg::Group();
    annoGroup->addChild( labelGroup );


    // Style our labels:
    Style labelStyle;
    labelStyle.getOrCreate<TextSymbol>()->alignment() = TextSymbol::ALIGN_CENTER_CENTER;
    labelStyle.getOrCreate<TextSymbol>()->fill()->color() = Color::Yellow;

    // A lat/long SRS for specifying points.
    const SpatialReference* geoSRS = mapNode->getMapSRS()->getGeographicSRS();

    //--------------------------------------------------------------------

    // A series of place nodes (an icon with a text label)
    {
        Style pm;
        pm.getOrCreate<IconSymbol>()->url()->setLiteral( "../data/placemark32.png" );
        pm.getOrCreate<IconSymbol>()->declutter() = true;
        pm.getOrCreate<TextSymbol>()->halo() = Color("#5f5f5f");
        pm.getOrCreate<TextSymbol>()->encoding()=TextSymbol::ENCODING_UTF8;
        pm.getOrCreate<TextSymbol>()->font()="simhei.ttf";

        // bunch of pins:
        labelGroup->addChild( new PlaceNode(GeoPoint(geoSRS, -74.00, 40.71), "New York"      , pm));

        labelGroup->addChild( new PlaceNode(GeoPoint(geoSRS, 116.42472, 39.90556), "北京" , pm));

        // test with an LOD:
        osg::LOD* lod = new osg::LOD();
        lod->addChild( new PlaceNode(GeoPoint(geoSRS, 14.68, 50.0), "Prague", pm), 0.0, 2e6);
        labelGroup->addChild( lod );

        // absolute altitude:
        labelGroup->addChild( new PlaceNode(GeoPoint(geoSRS, -87.65, 41.90, 1000, ALTMODE_ABSOLUTE), "Chicago", pm));
    }

    //--------------------------------------------------------------------
	//添加模型
	{
		osg::Node* model = osgDB::readNodeFile("../data/cow.osgt");
        //通过原始模型添加，需要给模型一个染色器
        osgEarth::Registry::shaderGenerator().run(model);
		//osg中光照只会对有法线的模型起作用，而模型经过缩放后法线是不会变得，
		//所以需要手动设置属性，让法线随着模型大小变化而变化。GL_NORMALIZE 或 GL_RESCALE_NORMAL
		model->getOrCreateStateSet()->setMode(GL_RESCALE_NORMAL, osg::StateAttribute::ON);

		osg::Matrix Lmatrix;
		geoSRS->getEllipsoid()->computeLocalToWorldTransformFromLatLongHeight(osg::DegreesToRadians(40.0), osg::DegreesToRadians(116.0), 0.0, Lmatrix);
		//放大一些，可以看到
		Lmatrix.preMult(osg::Matrix::scale(osg::Vec3(10000, 10000, 10000)));

		osg::MatrixTransform* mt = new osg::MatrixTransform;
		mt->setMatrix(Lmatrix);
		mt->addChild(model);
		annoGroup->addChild(mt);
		cout <<"cow r:" <<mt->getBound().radius() <<endl;
	}
    ModelNode *cessna = nullptr;
    {
        Style style;
        //这里必须设置自动缩放
        style.getOrCreate<ModelSymbol>()->autoScale() = false;
        style.getOrCreate<ModelSymbol>()->url()->setLiteral("../data/cessna.osg.1000.scale");
        //style.getOrCreate<ModelSymbol>()->url()->setLiteral("../data/cessna.osg");
        ModelNode* modelNode = new ModelNode(mapNode, style);
        cessna = modelNode;
        modelNode->setPosition(GeoPoint(geoSRS, 121., 38., 50000, ALTMODE_ABSOLUTE));
        annoGroup->addChild(modelNode);
		cout <<"cessna r:" <<modelNode->getBound().radius() <<endl;
    }
    viewer.setSceneData( root );
	//viewer.realize();  
    return viewer.run();
}
