#include "textdraw.h"
#include <pugixml.hpp>

FontRef loadFont(char const* fontFilename)
{
    FileData xmlFile = fileRead(fontFilename);
    rgAssert(xmlFile.isValid);
    
    pugi::xml_document xmlDoc;
    pugi::xml_parse_result parseResult = xmlDoc.load_buffer(xmlFile.data, xmlFile.dataSize);
    
    rgAssert(parseResult.status == pugi::status_ok);
    
    FontRef fontRef = eastl::shared_ptr<Font>(rgNew(Font), unloadFont);
    
    pugi::xml_node fontNode = xmlDoc.child("font");
    pugi::xml_node commonNode = fontNode.child("common");
    
    fontRef->lineHeight = commonNode.attribute("lineHeight").as_uint();
    fontRef->base = commonNode.attribute("base").as_uint();
    
    unsigned fontAtlasWidth = commonNode.attribute("scaleW").as_uint();
    unsigned fontAtlasHeight = commonNode.attribute("scaleH").as_uint();
    
    // Only support 1 font atlas
    rgAssert(commonNode.attribute("pages").as_uint() == 1);
    
    const char* page0Filename = fontNode.child("pages").child("page").attribute("file").value();
    
    // TODO: use file path utilities function to construct path
    char page0Filepath[1024] = "fonts/";
    strncat(page0Filepath, page0Filename, rgArrayCount(page0Filepath) - 7);
    
    ImageRef page0Image = loadImage(page0Filepath);
    fontRef->atlasTexture = GfxTexture::create(page0Filename, GfxTextureDim_2D, page0Image->width, page0Image->height, page0Image->format, GfxTextureMipFlag_1Mip, GfxTextureUsage_ShaderRead, page0Image->slices);
    
    // Populate glyph db
    pugi::xml_node charsNode = fontNode.child("chars");
    unsigned glyphCount = charsNode.attribute("count").as_uint();
    fontRef->glyphCount = glyphCount;
    
    fontRef->glyphs.reserve(glyphCount);
    pugi::xml_node charNode = charsNode.child("char");
    while(charNode)
    {
        unsigned x, y, w, h;
        x = charNode.attribute("x").as_uint();
        y = charNode.attribute("y").as_uint();
        w = charNode.attribute("width").as_uint();
        h = charNode.attribute("height").as_uint();
        
        QuadUV uv = createQuadUV(x, y, w, h, fontAtlasWidth, fontAtlasHeight);
        
        unsigned codepoint = charNode.attribute("id").as_uint();
        
        Glyph gly = {};
        gly.id = codepoint;
        gly.xOffset = charNode.attribute("xoffset").as_uint();
        gly.yOffset = charNode.attribute("yoffset").as_uint();
        gly.xAdvance = charNode.attribute("xadvance").as_uint();
        gly.uv = uv;
        
        fontRef->glyphs[codepoint] = gly;
        charNode = charNode.next_sibling();
    }

    fileFree(&xmlFile);
    
    return fontRef;
}

void unloadFont(Font* font)
{
    GfxTexture::destroy(font->atlasTexture);
}
