#include "mainwindow.h"
#include "txdexport.h"

#include "dirtools.h"

static rw::TexDictionary* RwTexDictionaryStreamRead( rw::Interface *rwEngine, CFile *stream )
{
    rw::TexDictionary *resultDict = NULL;

    rw::Stream *rwStream = RwStreamCreateTranslated( rwEngine, stream );

    if ( rwStream )
    {
        try
        {
            rw::RwObject *rwObj = rwEngine->Deserialize( rwStream );

            if ( rwObj )
            {
                try
                {
                    resultDict = rw::ToTexDictionary( rwEngine, rwObj );
                }
                catch( ... )
                {
                    rwEngine->DeleteRwObject( rwObj );

                    throw;
                }

                if ( !resultDict )
                {
                    rwEngine->DeleteRwObject( rwObj );
                }
            }
        }
        catch( ... )
        {
            rwEngine->DeleteStream( rwStream );

            throw;
        }
        
        rwEngine->DeleteStream( rwStream );
    }

    return resultDict;
}

static void ExportImagesFromDictionary(
    rw::TexDictionary *texDict, CFileTranslator *outputRoot,
    const filePath& txdFileName, const filePath& relPathFromRoot,
    MassExportModule::eOutputType outputType,
    const std::string& imgFormat
)
{
    rw::Interface *rwEngine = texDict->GetEngine();

    for ( rw::TexDictionary::texIter_t iter( texDict->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
    {
        rw::TextureBase *texHandle = iter.Resolve();

        if ( rw::Raster *texRaster = texHandle->GetRaster() )
        {
            // Construct the target filename.
            filePath targetFileName = relPathFromRoot;

            if ( outputType == MassExportModule::OUTPUT_PLAIN )
            {
                // We are a plain path, which is just the texture name appended.
            }
            else if ( outputType == MassExportModule::OUTPUT_TXDNAME )
            {
                // Also put the TexDictionary name before it.
                targetFileName += txdFileName;
                targetFileName += "_";
            }
            else if ( outputType == MassExportModule::OUTPUT_FOLDERS )
            {
                // Instead put it inside folders.
                targetFileName += txdFileName;
                targetFileName += "/";
            }
        
            targetFileName += texHandle->GetName();
            targetFileName += ".";
            
            std::string lower_ext = imgFormat;
            std::transform( lower_ext.begin(), lower_ext.end(), lower_ext.begin(), ::tolower );

            targetFileName += lower_ext;

            // Create the target stream.
            CFile *targetStream = outputRoot->Open( targetFileName, "wb" );

            if ( targetStream )
            {
                try
                {
                    rw::Stream *rwStream = RwStreamCreateTranslated( rwEngine, targetStream );

                    if ( rwStream )
                    {
                        try
                        {
                            // Write it!
                            try
                            {
                                if ( stricmp( imgFormat.c_str(), "RWTEX" ) == 0 )
                                {
                                    rwEngine->Serialize( texHandle, rwStream );
                                }
                                else
                                {
                                    texRaster->writeImage( rwStream, imgFormat.c_str() );
                                }
                            }
                            catch( rw::RwException& )
                            {
                                // If we failed to write it, just live with it.
                            }
                        }
                        catch( ... )
                        {
                            rwEngine->DeleteStream( rwStream );

                            throw;
                        }

                        rwEngine->DeleteStream( rwStream );
                    }
                }
                catch( ... )
                {
                    delete targetStream;

                    throw;
                }

                delete targetStream;
            }
        }
    }
}

struct _discFileSentry_txdexport
{
    MassExportModule *module;
    const MassExportModule::run_config *config;

    inline bool OnSingletonFile(
        CFileTranslator *sourceRoot, CFileTranslator *buildRoot, const filePath& relPathFromRoot,
        const filePath& fileName, const filePath& extention, CFile *sourceStream,
        bool isInArchive
    )
    {
        rw::Interface *rwEngine = module->GetEngine();

        bool anyWork = false;

        // Terminate if we are asked to.
        rw::CheckThreadHazards( rwEngine );

        try
        {
            // We just process TXD files.
            if ( extention.equals( "TXD", false ) == true )
            {
                // Send an appropriate status message.
                {
                    std::wstring statusFileName;

                    if ( isInArchive )
                    {
                        statusFileName += L"$";
                    }

                    statusFileName += relPathFromRoot.convert_unicode();

                    module->OnProcessingFile( statusFileName );
                }

                // Get the relative path to the file without the filename.
                filePath relPathFromRootWithoutFile;

                buildRoot->GetRelativePathFromRoot( relPathFromRoot, false, relPathFromRootWithoutFile );

                // For each texture that we find, export it as raw image.
                rw::TexDictionary *texDict = RwTexDictionaryStreamRead( rwEngine, sourceStream );

                if ( texDict )
                {
                    try
                    {
                        // Export everything inside of this.
                        ExportImagesFromDictionary(
                            texDict, buildRoot, fileName, relPathFromRootWithoutFile, config->outputType,
                            config->recImgFormat
                        );

                        anyWork = true;
                    }
                    catch( ... )
                    {
                        rwEngine->DeleteRwObject( texDict );

                        throw;
                    }

                    rwEngine->DeleteRwObject( texDict );
                }
            }
        }
        catch( rw::RwException& except )
        {
            // We ignore RenderWare errors.
        }

        // Done!
        return anyWork;
    }

    inline void OnArchiveFail( const filePath& fileName, const filePath& extention )
    {
        return;
    }
};

bool MassExportModule::ApplicationMain( const run_config& cfg )
{
    // We run through all TXD files we find and put them into the output root.
    CFileTranslator *gameRootTranslator = NULL;
    CFileTranslator *outputRootTranslator = NULL;

    bool gotGameRoot = obtainAbsolutePath( cfg.gameRoot.c_str(), gameRootTranslator, false );

    try
    {
        bool gotOutputRoot = obtainAbsolutePath( cfg.outputRoot.c_str(), outputRootTranslator, true );

        try
        {
            if ( gotGameRoot && gotOutputRoot )
            {
#if 0
                // Warn the user if there is a build root path conflict.
                // This usually means that certain files will not be processed due to recursion risk.
                if ( isBuildRootConflict( gameRootTranslator, outputRootTranslator ) )
                {
                    this->OnMessage( "build root conflict detected; might not process all files\n\n" );
                }
#endif

                gtaFileProcessor <_discFileSentry_txdexport> fileProc( this );

                fileProc.setUseCompressedIMGArchives( true );
                fileProc.setArchiveReconstruction( false );

                _discFileSentry_txdexport sentry;
                sentry.module = this;
                sentry.config = &cfg;

                fileProc.process( &sentry, gameRootTranslator, outputRootTranslator );
            }
        }
        catch( ... )
        {
            if ( gotOutputRoot )
            {
                delete outputRootTranslator;
            }

            throw;
        }

        if ( gotOutputRoot )
        {
            delete outputRootTranslator;
        }
    }
    catch( ... )
    {
        if ( gotGameRoot )
        {
            delete gameRootTranslator;
        }

        throw;
    }
    
    if ( gotGameRoot )
    {
        delete gameRootTranslator;
    }

    // Done.
    return true;
}