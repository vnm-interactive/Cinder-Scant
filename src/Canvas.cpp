﻿#include "cinder/Rand.h"
#include "cinder/ImageIo.h"

#include "Canvas.h"
#include "Config.h"
#include "Utils.h"
#include "Resources.h"

using namespace ci;
using namespace ci::app;
using namespace std;

Canvas::Canvas( ci::Vec2f _pos, ci::Vec2f _size ) {
	msaaFormat.setSamples( 4 ); // enable 4x MSAA
	msaaFormat.enableDepthBuffer( false );
	msaaFormat.setColorInternalFormat( GL_RGBA8 );

	setColor( Config::CANVAS_COLOR );
	setLineColor( Color::hex( Config::DEFAULT_LINE_COLOR ) );
	setSize( _size.x, _size.y );
	setPosition( _pos.x, _pos.y );

	setLineSize( (float)Config::DEFAULT_LINE_SIZE );
	setLineVariance( (float)Config::DEFAULT_LINE_VARIANCE );
	setLineBlendMode( LINE_BLEND_NORMAL );

	canvasTexture = gl::Texture::create( loadImage( loadResource( CANVAS_TEXTURE ) ) );
	canvasTexture->setWrap( GL_REPEAT, GL_REPEAT );

	saveUndo();
};

void Canvas::initFbos() {
	if(fbo){
		gl::Fbo tempFbo = gl::Fbo( (int)size.x, (int)size.y, msaaFormat );
		tempFbo.getTexture().setFlipped(true);
		tempFbo.bindFramebuffer();
		gl::clear( Color( 1.0f, 1.0f, 1.0f ) ); 
		//gl::clear( Color::hex( Config::CANVAS_COLOR ) ); 
		tempFbo.unbindFramebuffer();

		fbo.blitTo( tempFbo, fbo.getBounds(), fbo.getBounds() );
		fbo = gl::Fbo( (int)size.x, (int)size.y, msaaFormat );
		fbo.getTexture().setFlipped(true);
		clearFbo( fbo );
		tempFbo.blitTo( fbo, tempFbo.getBounds(), tempFbo.getBounds() );
	}else{
		fbo = gl::Fbo( (int)size.x, (int)size.y, msaaFormat );
		fbo.getTexture().setFlipped(true);
		clearFbo( fbo );
	}

	undoFbo = gl::Fbo( (int)size.x, (int)size.y, msaaFormat );
	undoFbo.getTexture().setFlipped(true);
	clearFbo( undoFbo );
}

Area Canvas::getBounds(){
	return fbo.getBounds() + pos;
}

void Canvas::setSize(ci::Vec2f size){
	setSize( size.x, size.y );
}
void Canvas::setSize(float width, float height){
	size.set(width, height);
	yOffset = size.y - getWindowHeight() + pos.y;

	initFbos();
	clearUndo();
	saveUndo();
}
Vec2f Canvas::getSize(){
	return size;
}

void Canvas::setPosition(float x, float y){
	pos.set( x, y );
	yOffset = size.y - getWindowHeight() + pos.y;
}
Vec2f Canvas::getPosition(){
	return pos;
}

void Canvas::setColor(float r, float g, float b){
	color.set( r, g, b );
}
void Canvas::setColor(int hexColor){
	Color c = Color::hex( hexColor );
	color.set( c.r, c.g, c.b );
}
Color Canvas::getColor(){
	return color;
}

void Canvas::setLineColor(ci::Color color){
	lineColor.set( color.r, color.g, color.b );
}
void Canvas::setLineColor(float r, float g, float b){
	lineColor.set( r, g, b );
}
Color Canvas::getLineColor(){
	return lineColor;
}

void Canvas::setLineSize(float value){
	lineSize = value;
}
float Canvas::getLineSize(){
	return lineSize;
}

void Canvas::setLineVariance(float value){
	lineVariance = value;
}
float Canvas::getLineVariance(){
	return lineVariance;
}

void Canvas::setLineBlendMode(LineBlendMode bm){
	lineBlendMode = bm;
}
LineBlendMode Canvas::getLineBlendMode(){
	return lineBlendMode;
}

//*******************************************************************************
// DRAW
//*******************************************************************************

void Canvas::draw(){
	gl::color(color);
	gl::draw( fbo.getTexture(), pos );

	// blend canvas texture on top
	glEnable (GL_BLEND);
	glBlendFunc( GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA ); 
	gl::draw( canvasTexture, getBounds() );
	glDisable(GL_BLEND);

	//gl::draw( undoFbo.getTexture(), Area(0,0,200,100) ); //visualize undofbo
}

void Canvas::clear(){
	clearUndo();
	clearFbo( fbo );
}

void Canvas::clearFbo( ci::gl::Fbo target ){
	target.bindFramebuffer();
	gl::clear( color ); 
	target.unbindFramebuffer();
}

void Canvas::addLine( Vec2f endPos ){
	addLine( lastEndPos, endPos );
}

void Canvas::addLine( Vec2f startPos, Vec2f endPos ){
	lastEndPos = endPos;

	Line* line = new Line();
	line->startPos = Vec2f( startPos.x-pos.x, startPos.y-yOffset ); //include positon offset
	line->endPos = Vec2f( endPos.x-pos.x, endPos.y-yOffset ); //include positon offset
	line->color = lineColor;
	line->blendMode = lineBlendMode;

	float v = lineVariance * lineSize;
	float variance = randFloat( 0, v ) - v/2;
	float size = lineSize + variance;
	line->width = size > Config::MIN_LINE_SIZE ? size : Config::MIN_LINE_SIZE;
	linesList.push_back( line );

	drawLineToFbo( fbo, line );
}

void Canvas::drawLineToFbo( gl::Fbo fbo, Line* line){
	fbo.bindFramebuffer();

	if( line->blendMode == LINE_BLEND_MULTIPLY ){
		glEnable (GL_BLEND);
		glBlendEquation(GL_MIN);
	}

	gl::color( line->color );

	float w = line->startPos.distance( line->endPos );
	float h = line->width;
	Vec2f center = ( line->startPos + line->endPos ) / 2;
	Rectf lineRect = Rectf( 0, 0, w, h );
	lineRect.offset( Vec2f( -w/2, -h/2)  );
	float radius = h/2;
	float angle = Utils::angleBetweenPoints( line->startPos, line->endPos );

	// draw line
	Matrix44f transform;
	transform.translate( Vec3f(center.x, center.y, 0.0f) );
	transform.rotate( Vec3f(0.0f, 0.0f, toRadians(angle) ) );
	gl::pushModelView();
	gl::multModelView( transform );
	gl::drawSolidRect( lineRect );
	gl::popModelView();

	// draw round caps
	gl::drawSolidCircle( line->startPos, radius );
	gl::drawSolidCircle( line->endPos, radius );

	if( line->blendMode == LINE_BLEND_MULTIPLY ){
		glBlendEquation(GL_FUNC_ADD);
		glDisable(GL_BLEND);
	}

	fbo.unbindFramebuffer();
}


//*******************************************************************************
// UNDO
//*******************************************************************************

void Canvas::saveUndo(){
	strokeIndexList.push_back( linesList.size() );
	trimUndoLength();

	//ci::app::console() << "undo steps: " << strokeIndexList.size() << ", lines: " << linesList.size() << std::endl;
}

void Canvas::undo(){

	unsigned int strokeIndex = 0;

	if( strokeIndexList.size() > 0 ){
		strokeIndexList.pop_back();
		
		if( strokeIndexList.size() > 0 ) 
			strokeIndex = strokeIndexList.back();

		if( linesList.size() > 0 ){
			while( linesList.size() > strokeIndex ){
				delete linesList.back();
				linesList.pop_back();
			}
		}
	}

	// draw remining lines on top of last fbo texture
	undoFbo.blitTo( fbo, undoFbo.getBounds(), fbo.getBounds() ); // start from last fbo texture
	if( linesList.size() > 0 ){
		for (std::deque<Line*>::iterator it = linesList.begin(); it!=linesList.end(); ++it)
			drawLineToFbo( fbo, *it );
	}else{
		//undo line list empty, just update fbo texture
		fbo.bindFramebuffer();
		fbo.unbindFramebuffer();
	}

	//ci::app::console() << "undo steps: " << strokeIndexList.size() << ", lines: " << linesList.size() << std::endl;
}

void Canvas::clearUndo(){
	while( linesList.size() > 0){
		delete linesList.back();
		linesList.pop_back();
	}
	while( strokeIndexList.size() > 0){
		strokeIndexList.pop_back();
	}
	clearFbo( undoFbo );
}

void Canvas::trimUndoLength(){
	while( strokeIndexList.size() > Config::UNDO_LENGTH ){
		unsigned int firstStrokeIndex = strokeIndexList.front();
		unsigned int remainingLineCount = linesList.size() - firstStrokeIndex;

		// draw trimmed stroke to last undo fbo texture
		while( linesList.size() > remainingLineCount ){
			drawLineToFbo( undoFbo, linesList.front() );
			delete linesList.front();
			linesList.pop_front();
		}

		// update stroke indices and remove first trimmed one
		for (std::deque<unsigned int>::iterator it = strokeIndexList.begin(); it!=strokeIndexList.end(); ++it)
			*it -= firstStrokeIndex; 
		strokeIndexList.pop_front();
	}
}