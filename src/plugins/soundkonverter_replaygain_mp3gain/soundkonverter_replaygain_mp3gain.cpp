
#include "mp3replaygainglobal.h"

#include "soundkonverter_replaygain_mp3gain.h"

#include <KDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>


Mp3GainPluginItem::Mp3GainPluginItem( QObject *parent )
    : ReplayGainPluginItem( parent )
{}

Mp3GainPluginItem::~Mp3GainPluginItem()
{}


soundkonverter_replaygain_mp3gain::soundkonverter_replaygain_mp3gain( QObject *parent, const QStringList& args  )
    : ReplayGainPlugin( parent )
{
    Q_UNUSED(args)

    binaries["mp3gain"] = "";

    allCodecs += "mp3";

    KSharedConfig::Ptr conf = KGlobal::config();
    KConfigGroup group;

    group = conf->group( "Plugin-"+name() );
    tagMode = group.readEntry( "tagMode", 0 );
    modifyAudioStream = group.readEntry( "modifyAudioStream", false );
    gainAdjustment = group.readEntry( "gainAdjustment", 0.0 );
}

soundkonverter_replaygain_mp3gain::~soundkonverter_replaygain_mp3gain()
{}

QString soundkonverter_replaygain_mp3gain::name()
{
    return global_plugin_name;
}

QList<ReplayGainPipe> soundkonverter_replaygain_mp3gain::codecTable()
{
    QList<ReplayGainPipe> table;
    ReplayGainPipe newPipe;

    newPipe.codecName = "mp3";
    newPipe.rating = 100;
    newPipe.enabled = ( binaries["mp3gain"] != "" );
    newPipe.problemInfo = standardMessage( "replygain_codec,backend", "mp3", "mp3gain" ) + "\n" + standardMessage( "install_patented_backend", "mp3gain" );
    table.append( newPipe );

    return table;
}

bool soundkonverter_replaygain_mp3gain::isConfigSupported( ActionType action, const QString& codecName )
{
    Q_UNUSED(action)
    Q_UNUSED(codecName)

    return true;
}

void soundkonverter_replaygain_mp3gain::showConfigDialog( ActionType action, const QString& codecName, QWidget *parent )
{
    Q_UNUSED(action)
    Q_UNUSED(codecName)

    if( !configDialog.data() )
    {
        configDialog = new KDialog( parent );
        configDialog.data()->setCaption( i18n("Configure %1").arg(global_plugin_name)  );
        configDialog.data()->setButtons( KDialog::Ok | KDialog::Cancel | KDialog::Default );

        QWidget *configDialogWidget = new QWidget( configDialog.data() );
        QVBoxLayout *configDialogBox = new QVBoxLayout( configDialogWidget );

        QHBoxLayout *configDialogBox1 = new QHBoxLayout();
        QLabel *configDialogTagModeLabel = new QLabel( i18n("Use tag format:"), configDialogWidget );
        configDialogBox1->addWidget( configDialogTagModeLabel );
        configDialogTagModeComboBox = new QComboBox( configDialogWidget );
        configDialogTagModeComboBox->addItem( "APE" );
        configDialogTagModeComboBox->addItem( "ID3v2" );
        configDialogBox1->addWidget( configDialogTagModeComboBox );
        configDialogBox->addLayout( configDialogBox1 );

        QHBoxLayout *configDialogBox3 = new QHBoxLayout();
        QLabel *configDialogGainAdjustmentLabel = new QLabel( i18n("Adjust gain:"), configDialogWidget );
        configDialogBox3->addWidget( configDialogGainAdjustmentLabel );
        configDialogGainAdjustmentSpinBox = new QDoubleSpinBox( configDialogWidget );
        configDialogGainAdjustmentSpinBox->setRange( -99, 99 );
        configDialogGainAdjustmentSpinBox->setSuffix( " " + i18nc("decibel","dB") );
        configDialogGainAdjustmentSpinBox->setToolTip( i18n("Lower or raise the suggested gain") );
        configDialogBox3->addWidget( configDialogGainAdjustmentSpinBox );
        configDialogBox->addLayout( configDialogBox3 );

        QHBoxLayout *configDialogBox2 = new QHBoxLayout();
        configDialogModifyAudioStreamCheckBox = new QCheckBox( i18n("Modify audio stream"), configDialogWidget );
        configDialogModifyAudioStreamCheckBox->setToolTip( i18n("Write gain adjustments directly into the encoded data. That way the adjustment works with all mp3 players.\nUndoing the changes is still possible since correction data will be written as well.") );
        configDialogBox2->addWidget( configDialogModifyAudioStreamCheckBox );
        configDialogBox->addLayout( configDialogBox2 );

        configDialog.data()->setMainWidget( configDialogWidget );
        connect( configDialog.data(), SIGNAL( okClicked() ), this, SLOT( configDialogSave() ) );
        connect( configDialog.data(), SIGNAL( defaultClicked() ), this, SLOT( configDialogDefault() ) );
    }
    configDialogTagModeComboBox->setCurrentIndex( tagMode );
    configDialogModifyAudioStreamCheckBox->setChecked( modifyAudioStream );
    configDialogGainAdjustmentSpinBox->setValue( gainAdjustment );
    configDialog.data()->show();
}

void soundkonverter_replaygain_mp3gain::configDialogSave()
{
    if( configDialog.data() )
    {
        tagMode = configDialogTagModeComboBox->currentIndex();
        modifyAudioStream = configDialogModifyAudioStreamCheckBox->isChecked();
        gainAdjustment = configDialogGainAdjustmentSpinBox->value();

        KSharedConfig::Ptr conf = KGlobal::config();
        KConfigGroup group;

        group = conf->group( "Plugin-"+name() );
        group.writeEntry( "tagMode", tagMode );
        group.writeEntry( "modifyAudioStream", modifyAudioStream );
        group.writeEntry( "gainAdjustment", gainAdjustment );

        configDialog.data()->deleteLater();
    }
}

void soundkonverter_replaygain_mp3gain::configDialogDefault()
{
    if( configDialog.data() )
    {
        configDialogTagModeComboBox->setCurrentIndex( 0 );
        configDialogModifyAudioStreamCheckBox->setChecked( false );
        configDialogGainAdjustmentSpinBox->setValue( 0.0 );
    }
}

bool soundkonverter_replaygain_mp3gain::hasInfo()
{
    return false;
}

void soundkonverter_replaygain_mp3gain::showInfo( QWidget *parent )
{
    Q_UNUSED(parent)
}

unsigned int soundkonverter_replaygain_mp3gain::apply( const KUrl::List& fileList, ReplayGainPlugin::ApplyMode mode )
{
    if( fileList.count() <= 0 )
        return BackendPlugin::UnknownError;

    Mp3GainPluginItem *newItem = new Mp3GainPluginItem( this );
    newItem->id = lastId++;
    newItem->process = new KProcess( newItem );
    newItem->process->setOutputChannelMode( KProcess::MergedChannels );
    connect( newItem->process, SIGNAL(readyRead()), this, SLOT(processOutput()) );

    QStringList command;
    command += binaries["mp3gain"];
    if( mode == ReplayGainPlugin::Add )
    {
        command += "-k";
        if( modifyAudioStream )
        {
            command += "-a";
        }
        connect( newItem->process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(processExit(int,QProcess::ExitStatus)) );
    }
    else if( mode == ReplayGainPlugin::Force )
    {
        command += "-k";
        if( modifyAudioStream )
        {
            command += "-a";
        }
        command += "-s";
        command += "r";
        connect( newItem->process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(processExit(int,QProcess::ExitStatus)) );
    }
    else
    {
        command += "-u";
        connect( newItem->process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(undoProcessExit(int,QProcess::ExitStatus)) );
        newItem->undoFileList = fileList;
    }
    if( gainAdjustment != 0 )
    {
        command += "-d";
        command += QString::number(gainAdjustment);
    }
    if( mode == ReplayGainPlugin::Add || mode == ReplayGainPlugin::Force )
    {
        if( tagMode == 0 )
        {
            // APE tags
            command += "-s";
            command += "a";
        }
        else
        {
            // ID3v2 tags
            command += "-s";
            command += "i";
        }
    }
    foreach( const KUrl file, fileList )
    {
        command += "\"" + escapeUrl(file) + "\"";
    }

    newItem->process->clearProgram();
    newItem->process->setShellCommand( command.join(" ") );
    newItem->process->start();

    logCommand( newItem->id, command.join(" ") );

    backendItems.append( newItem );
    return newItem->id;
}

void soundkonverter_replaygain_mp3gain::undoProcessExit( int exitCode, QProcess::ExitStatus exitStatus )
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    Mp3GainPluginItem *item = 0;

    for( int i=0; i<backendItems.size(); i++ )
    {
        if( backendItems.at(i)->process == QObject::sender() )
        {
            item = (Mp3GainPluginItem*)backendItems.at(i);
            break;
        }
    }

    if( !item )
        return;

    if( item->undoFileList.count() <= 0 )
        return;

    if( item->process )
        item->process->deleteLater();

    item->process = new KProcess( item );
    item->process->setOutputChannelMode( KProcess::MergedChannels );
    connect( item->process, SIGNAL(readyRead()), this, SLOT(processOutput()) );
    connect( item->process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(processExit(int,QProcess::ExitStatus)) );

    QStringList command;
    command += binaries["mp3gain"];
    // APE tags
    command += "-s";
    command += "a";
    // ID3v2 tags
    command += "-s";
    command += "i";
    // delete tags
    command += "-s";
    command += "d";
    foreach( const KUrl file, item->undoFileList )
    {
        command += "\"" + escapeUrl(file) + "\"";
    }

    item->process->clearProgram();
    item->process->setShellCommand( command.join(" ") );
    item->process->start();

    logCommand( item->id, command.join(" ") );
}

float soundkonverter_replaygain_mp3gain::parseOutput( const QString& output )
{
    //  9% of 45218064 bytes analyzed
    // [1/10] 32% of 13066690 bytes analyzed

    float progress = -1.0f;

    QRegExp reg1("\\[(\\d+)/(\\d+)\\] (\\d+)%");
    QRegExp reg2("(\\d+)%");
    if( output.contains(reg1) )
    {
        float fraction = 1.0f/reg1.cap(2).toInt();
        progress = 100*(reg1.cap(1).toInt()-1)*fraction + reg1.cap(3).toInt()*fraction;
    }
    else if( output.contains(reg2) )
    {
        progress = reg2.cap(1).toInt();
    }

    // Applying mp3 gain change of -6 to /home/user/file.mp3...
    // Undoing mp3gain changes (6,6) to /home/user/file.mp3...
    // Deleting tag info of /home/user/file.mp3...
    QRegExp reg3("[Applying mp3 gain change|Undoing mp3gain changes|Deleting tag info]");
    if( progress == -1 && output.contains(reg3) )
    {
        progress = 0.0f;
    }

    return progress;
}

#include "soundkonverter_replaygain_mp3gain.moc"


