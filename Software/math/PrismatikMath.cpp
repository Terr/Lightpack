/*
 * LightpackMath.cpp
 *
 *	Created on: 29.09.2011
 *		Project: Lightpack
 *
 *	Copyright (c) 2011 Mike Shatohin, mikeshatohin [at] gmail.com
 *
 *	Lightpack a USB content-driving ambient lighting system
 *
 *	Lightpack is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Lightpack is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.	If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "PrismatikMath.hpp"
#include <algorithm>
#include "../src/debug.h"

namespace PrismatikMath
{
	//Observer= 2°, Illuminant= D65
	const float refX = 95.047f;
	const float refY = 100.000f;
	const float refZ = 108.883f;

	template<typename A, typename T>
	T withinRange(A x, T min, T max)
	{
		T result;
		if (x < min)
			result = min;
		else if (x > max)
			result = max;
		else
			result = x;
		return result;
	}

	void gammaCorrection(double gamma, StructRgb & eRgb)
	{
		eRgb.r = 4095 * pow(eRgb.r / 4095.0, gamma);
		eRgb.g = 4095 * pow(eRgb.g / 4095.0, gamma);
		eRgb.b = 4095 * pow(eRgb.b / 4095.0, gamma);
	}

	void brightnessCorrection(unsigned int brightness, StructRgb & eRgb) {

		// brightness -- must be in percentage (0..100%)
		eRgb.r = (brightness / 100.0) * eRgb.r;
		eRgb.g = (brightness / 100.0) * eRgb.g;
		eRgb.b = (brightness / 100.0) * eRgb.b;
	}

	void maxCorrection(unsigned int max, StructRgb & eRgb) {
		if (eRgb.r > max) eRgb.r = max;
		if (eRgb.g > max) eRgb.g = max;
		if (eRgb.b > max) eRgb.b = max;
	}



	int getValueHSV(const QRgb rgb) {
		return max(rgb);
	}

	int max(const QRgb rgb) {
		return std::max( std::max( qRed(rgb), qGreen(rgb)), qBlue(rgb));
	}

	int min(const QRgb rgb) {
		return std::min( std::min( qRed(rgb), qGreen(rgb)), qBlue(rgb));
	}

	int getChromaHSV(const QRgb rgb) {
		return max(rgb) - min(rgb);
	}

	QRgb withValueHSV(const QRgb rgb, int value) {

		int currentValue = getValueHSV(rgb);

		if (currentValue == 0) {
			return qRgb(value, value, value);
		}

		double m = double(value)/currentValue;
		return qRgb(round(qRed(rgb)*m), round(qGreen(rgb)*m), round(qBlue(rgb)*m));

	}

	QRgb withChromaHSV(const QRgb rgb, int chroma) {
		int currentChroma = getChromaHSV(rgb);

		if (currentChroma == 0 ) {
			return rgb;
		}

		if (chroma < 0) chroma = 0;

		int currentMax = max(rgb);

		if (chroma == 0)
			return qRgb(currentMax, currentMax, currentMax);


		double m = 1 - double(chroma)/currentChroma;


		int r = qRed(rgb)	+ (currentMax - qRed(rgb))*m;
		int g = qGreen(rgb) + (currentMax - qGreen(rgb))*m;
		int b = qBlue(rgb)	+ (currentMax - qBlue(rgb))*m;

		if (r < 0) r = 0;
		if (g < 0) g = 0;
		if (b < 0) b = 0;

		return qRgb(r,g,b);
	}

	StructRgb avgColor(const QList<StructRgb> &colors) {
		StructRgb result;
		if (colors.size() > 0) {
			for (int i = 0; i < colors.size(); ++i) {
				result.r += colors[i].r;
				result.g += colors[i].g;
				result.b += colors[i].b;
			}
			result.r /= colors.size();
			result.g /= colors.size();
			result.b /= colors.size();
		}
		return result;
	}

	StructXyz toXyz(const StructRgb & rgb) {
		//12bit RGB from 0 to 4095
		double r = ( rgb.r / 4095.0 );
		double g = ( rgb.g / 4095.0 );
		double b = ( rgb.b / 4095.0 );

		if ( r > 0.04045 )
			r = pow( (r + 0.055)/1.055, 2.4);
		else
			r = r / 12.92;

		if ( g > 0.04045 )
			g = pow( (g + 0.055)/1.055, 2.4);
		else
			g = g / 12.92;

		if ( b > 0.04045 )
			b = pow( (b + 0.055)/1.055, 2.4);
		else
			b = b / 12.92;

		r = r * 100;
		g = g * 100;
		b = b * 100;

		//Observer. = 2°, Illuminant = D65
		StructXyz result;
		result.x = r * 0.4124 + g * 0.3576 + b * 0.1805;
		result.y = r * 0.2126 + g * 0.7152 + b * 0.0722;
		result.z = r * 0.0193 + g * 0.1192 + b * 0.9505;

		return result;
	}

	StructXyz toXyz(const StructLab &lab) {
		double y = (lab.l + 16.0) / 116.0;
		double x = lab.a / 500.0 + y;
		double z = y - lab.b / 200.0;

		if ( y*y*y > 0.008856 )
			y = y*y*y;
		else
			y = ( y - 16.0 / 116 ) / 7.787;

		if ( x*x*x > 0.008856 )
			x = x*x*x;
		else
			x = ( x - 16.0 / 116 ) / 7.787;

		if ( z*z*z > 0.008856 )
			z = z*z*z;
		else
			z = ( z - 16.0 / 116 ) / 7.787;

		StructXyz xyz;

		xyz.x = refX * x;		//ref_X =	95.047		Observer= 2°, Illuminant= D65
		xyz.y = refY * y;		//ref_Y = 100.000
		xyz.z = refZ * z;		//ref_Z = 108.883

		return xyz;
	}

	StructLab toLab(const StructXyz &xyz) {

		double x = xyz.x / refX;			//ref_X =	95.047	Observer= 2°, Illuminant= D65
		double y = xyz.y / refY;			//ref_Y = 100.000
		double z = xyz.z / refZ;			//ref_Z = 108.883

		if ( x > 0.008856 )
			x = pow(x, 1.0/3);
		else
			x = (7.787 * x) + ( 16.0 / 116 );

		if ( y > 0.008856 )
			y = pow(y, 1.0/3);
		else
			y = (7.787 * y) + (16.0 / 116);

		if ( z > 0.008856 )
			z = pow(z, 1.0/3);
		else
			z = (7.787 * z) + (16.0 / 116);

		StructLab result;
		result.l = round((116 * y) - 16);
		result.a = withinRange<double, char>(round(500 * ( x - y )), -128, 127);
		result.b = withinRange<double, char>(round(200 * ( y - z )), -128, 127);

		return result;
	}

	StructLab toLab(const StructRgb &rgb) {
		return toLab(toXyz(rgb));
	}

	StructRgb toRgb(const StructXyz &xyz) {
		double x = xyz.x / 100;		//X from 0 to	95.047		(Observer = 2°, Illuminant = D65)
		double y = xyz.y / 100;		//Y from 0 to 100.000
		double z = xyz.z / 100;		//Z from 0 to 108.883

		double r = x *	3.2406 + y * -1.5372 + z * -0.4986;
		double g = x * -0.9689 + y *	1.8758 + z *	0.0415;
		double b = x *	0.0557 + y * -0.2040 + z *	1.0570;

		if ( r > 0.0031308 )
			r = 1.055 * pow(r, (1 / 2.4)) - 0.055;
		else
			r = 12.92 * r;

		if ( g > 0.0031308 )
			g = 1.055 * pow(g, (1 / 2.4)) - 0.055;
		else
			g = 12.92 * g;

		if ( b > 0.0031308 )
			b = 1.055 * pow(b, (1 / 2.4)) - 0.055;
		else
			b = 12.92 * b;

		r = withinRange<double, double>(r, 0, 1);
		g = withinRange<double, double>(g, 0, 1);
		b = withinRange<double, double>(b, 0, 1);

		StructRgb eRgb;

		eRgb.r = round(r*4095);
		eRgb.g = round(g*4095);
		eRgb.b = round(b*4095);

		return eRgb;
	}

	StructRgb toRgb(const StructLab &lab) {
		return toRgb(toXyz(lab));
	}
	
	
	// the following conversions are copied from https://github.com/berendeanicolae/ColorSpace
	// slight adjustments were made to prevent RGB values from over/underflowing
	
    void RgbToXyz(StructRgb const& rgb, StructXyz& result)
    {
        double r = rgb.r / 255.0;
        double g = rgb.g / 255.0;
        double b = rgb.b / 255.0;
    
        r = ((r > 0.04045) ? pow((r + 0.055) / 1.055, 2.4) : (r / 12.92)) * 100.0;
        g = ((g > 0.04045) ? pow((g + 0.055) / 1.055, 2.4) : (g / 12.92)) * 100.0;
        b = ((b > 0.04045) ? pow((b + 0.055) / 1.055, 2.4) : (b / 12.92)) * 100.0;
    
        result.x = r*0.4124564 + g*0.3575761 + b*0.1804375;
        result.y = r*0.2126729 + g*0.7151522 + b*0.0721750;
        result.z = r*0.0193339 + g*0.1191920 + b*0.9503041;
    }
    
    void XyzToLab(StructXyz const& xyz, StructLabF& result)
    {
        double x = xyz.x / 95.047;
        double y = xyz.y / 100.00;
        double z = xyz.z / 108.883;
    
        x = (x > 0.008856) ? cbrt(x) : (7.787 * x + 16.0 / 116.0);
        y = (y > 0.008856) ? cbrt(y) : (7.787 * y + 16.0 / 116.0);
        z = (z > 0.008856) ? cbrt(z) : (7.787 * z + 16.0 / 116.0);
    
        result.l = (116.0 * y) - 16;
        result.a = 500 * (x - y);
        result.b = 200 * (y - z);
    }
    
    void LabToXyz(StructLabF const& lab, StructXyz& result)
    {
        double y = (lab.l + 16.0) / 116.0;
        double x = lab.a / 500.0 + y;
        double z = y - lab.b / 200.0;
    
        double x3 = x*x*x;
        double y3 = y*y*y;
        double z3 = z*z*z;
    
        result.x = ((x3 > 0.008856) ? x3 : ((x - 16.0 / 116.0) / 7.787)) * 95.047;
        result.y = ((y3 > 0.008856) ? y3 : ((y - 16.0 / 116.0) / 7.787)) * 100.0;
        result.z = ((z3 > 0.008856) ? z3 : ((z - 16.0 / 116.0) / 7.787)) * 108.883;
    }
    
    void XyzToRgb(StructXyz const& xyz, StructRgb& result)
    {
        double x = xyz.x / 100.0;
        double y = xyz.y / 100.0;
        double z = xyz.z / 100.0;
    
        double r = x * 3.2404542 + y * -1.5371385 + z * -0.4985314;
        double g = x * -0.9692660 + y * 1.8760108 + z * 0.0415560;
        double b = x * 0.0556434 + y * -0.2040259 + z * 1.0572252;
			
		if(r < 0)
		{
			r = 0;
		}
		if(g < 0)
		{
			g = 0;
		}
		if(b < 0)
		{
			b = 0;
		}
				
		double pow_r = (1.055*pow(r, 1 / 2.4) - 0.055);
		double pow_g = (1.055*pow(g, 1 / 2.4) - 0.055);
		double pow_b = (1.055*pow(b, 1 / 2.4) - 0.055);
		
		if(pow_r > 1)
		{
			pow_r = 1;
		}
		if(pow_g > 1)
		{
			pow_g = 1;
		}
		if(pow_b > 1)
		{
			pow_b = 1;
		}
        result.r = ((r > 0.0031308) ? pow_r : (12.92*r)) * 255.0;
        result.g = ((g > 0.0031308) ? pow_g : (12.92*g)) * 255.0;
        result.b = ((b > 0.0031308) ? pow_b : (12.92*b)) * 255.0;
    }
}
