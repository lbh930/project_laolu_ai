// Copyright Epic Games, Inc. All Rights Reserved.

const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const fs = require('fs');

// 1) 自动发现 src 下所有 .html 页面，建立 "同名 .ts 入口"
const pages = fs
  .readdirSync('./src', { withFileTypes: true })
  .filter((item) => !item.isDirectory())
  .filter((item) => path.parse(item.name).ext === '.html')
  .map((htmlFile) => path.parse(htmlFile.name).name);

// 2) 构造 entry：{ page: ./src/page.ts }
const entry = pages.reduce((config, page) => {
  config[page] = `./src/${page}.ts`;
  return config;
}, {});

module.exports = {
  entry,

  // 3) 为每个页面各自注入对应 chunk
  plugins: [].concat(
    pages.map(
      (page) =>
        new HtmlWebpackPlugin({
          title: `${page}`,
          template: `./src/${page}.html`,
          filename: `${page}.html`,
          chunks: [page, `${page}-vendor`], // 只注入自身入口及各自 vendor（见下方 cacheGroups）
          inject: 'body',
        })
    )
  ),

  module: {
    rules: [
      {
        test: /\.tsx?$/,
        loader: 'ts-loader',
        exclude: [/node_modules/],
        options: {
          // 保持你原本用的 tsconfig.esm.json
          configFile: 'tsconfig.esm.json',
        },
      },
      {
        test: /\.html$/i,
        use: 'html-loader',
      },
      {
        test: /\.css$/,
        type: 'asset/resource',
        generator: { filename: 'css/[name][ext]' },
      },
      {
        test: /\.(png|svg|jpg|jpeg|gif)$/i,
        type: 'asset/resource',
        generator: { filename: 'images/[name][ext]' },
      },
    ],
  },

  resolve: {
    extensions: ['.tsx', '.ts', '.js', '.svg', '.json'],
  },

  // 4) 输出规则：保持你原来的 UMD 产物和输出目录
  output: {
    filename: '[name].js',
    library: 'epicgames-frontend',
    libraryTarget: 'umd',
    path: process.env.WEBPACK_OUTPUT_PATH
      ? path.resolve(process.env.WEBPACK_OUTPUT_PATH)
      : path.resolve(__dirname, '../../../SignallingWebServer/www'),
    clean: true,
    globalObject: 'this',
    hashFunction: 'xxhash64',
  },

  // 5) 代码分割：将 Epic 与 Arcware 的依赖明确拆分到各自 vendor，避免互相打包
  optimization: {
    splitChunks: {
      chunks: 'all',
      cacheGroups: {
        // 仅给 player 入口提取 Epic 依赖
        'player-vendor': {
          test: /[\\/]node_modules[\\/]@epicgames-ps[\\/]/,
          name: 'player-vendor',
          chunks: (chunk) => chunk.name === 'player',
          enforce: true,
          priority: 30,
        },
        // 仅给 arcware 入口提取 Arcware 依赖
        'arcware-vendor': {
          test: /[\\/]node_modules[\\/]@arcware-cloud[\\/]/,
          name: 'arcware-vendor',
          chunks: (chunk) => chunk.name === 'player.arcware', // 如果你的文件名是 player.arcware.ts，请把 'arcware' 改成 'player.arcware'
          enforce: true,
          priority: 30,
        },
        // 回退的通用第三方库，防止重复
        vendors: {
          test: /[\\/]node_modules[\\/](?!@epicgames-ps)(?!@arcware-cloud)/,
          name(module, chunks, cacheGroupKey) {
            // 针对不同入口各自出一个通用 vendors，避免交叉引用
            const entryChunk = chunks.find((c) => pages.includes(c.name));
            return entryChunk ? `${entryChunk.name}-vendor` : 'vendors';
          },
          minChunks: 1,
          priority: 10,
          reuseExistingChunk: true,
        },
      },
    },
  },

  experiments: {
    futureDefaults: true,
  },

  // 6) 本地开发服务器，保持你的默认输出目录
  devServer: {
    static: {
      directory: path.join(__dirname, '../../../SignallingWebServer/www'),
    },
  },
};
