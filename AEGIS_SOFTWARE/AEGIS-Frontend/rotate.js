const sharp = require('sharp');
sharp('./assets/images/misc/Shahriar Jaman.jpg')
  .rotate(-90)
  .resize(500, 500, { fit: 'cover' })
  .jpeg({ quality: 80 })
  .toFile('./assets/images/team/Shahriar Jaman.jpg')
  .then(() => console.log('Done!'))
  .catch(console.error);
